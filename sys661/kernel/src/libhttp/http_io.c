#include "libhttp_internal.h"

int http_io_fn_1(u32 *a1)
{
  u32 *v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 *v11;
  int v12;
  int result;
  s64 v14;
  int v15;
  u32 v16[4];

  v2 = (u32 *)a1[233];
  v3 = 0;
  if ( v2 == NULL )
    v2 = (u32 *)a1[232];
  v4 = v2[13];
  if ( v4 == 5 )
    goto LABEL_39;
  if ( v4 == 6 )
  {
    v5 = 2;
    if ( a1[26] != 1 )
      goto LABEL_39;
    goto LABEL_6;
  }
  while ( 1 )
  {
    v5 = 2;
LABEL_6:
    v6 = 1;
    if ( v4 != 2 )
      return v3;
    if ( v2[16] != 1 )
    {
      v3 = http_opts_fn_28((int)v2);
      if ( v3 < 0 )
        goto LABEL_44;
    }
    v7 = sceNetInetSocket(2, 1, 0, v5, v6);
    v8 = v7;
    if ( v7 < 0 )
      break;
    v2[12] = v7;
    v2[13] = 2;
    if ( http_io_fn_8(v7, a1 + 34) < 0 )
      break;
    v16[0] = 1;
    if ( sceNetInetSetsockopt(v8, 0xFFFF, 4105, v16, 4) < 0 )
      break;
    v3 = http_io_fn_9(v8, v2 + 88, a1[41], v2 + 8, v2 + 9);
    if ( v3 < 0 )
      goto LABEL_44;
    v16[0] = 0;
    v3 = sceNetInetSetsockopt(v8, 0xFFFF, 4105, v16, 4);
    if ( v3 < 0 )
      break;
    if ( v2[1] != 1 )
    {
      v9 = a1[7];
LABEL_31:
      v14 = ((u64)(v14) & 0xFFFFFFFF00000000ULL) | (u32)(v9 == 1);
      v14 = ((u64)(v14) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v9 == 4) << 32);
      v15 = 6;
      if ( v14 == 0 )
        v15 = 5;
      v2[13] = v15;
      return v3;
    }
    if ( v2[48] == 1 && (http_core_fn_22((int)a1, (int)v2), (v3 = http_ssl_fn_12(a1, v8)) == -2143088523) )
    {
      http_io_fn_42(v2, a1);
      if ( v2[10] != -2143072156 || a1[215] != 407 )
      {
        v12 = v2[12];
        goto LABEL_25;
      }
      if ( http_ssl_fn_24((int)a1) <= 0 )
      {
        v3 = -2143088523;
        if ( a1[7] != 2 )
          a1[15] = 1;
        goto LABEL_24;
      }
LABEL_39:
      v3 = http_io_fn_2(v2);
      if ( v3 == 1 )
      {
        v3 = http_io_fn_8(v2[12], a1 + 34);
        if ( v3 < 0 )
          break;
        v4 = v2[13];
      }
      else
      {
        http_io_fn_24(v2);
        v4 = 2;
        v2[13] = 2;
      }
    }
    else
    {
      v10 = http_ssl_fn_13((int)a1, v8);
      v3 = v10;
      if ( v10 >= 0 )
      {
        v9 = a1[7];
        goto LABEL_31;
      }
      if ( v10 != -2143088523 )
        goto LABEL_24;
      if ( v2[10] != -2143072159 )
      {
        v11 = a1;
LABEL_23:
        http_io_fn_42(v2, v11);
        goto LABEL_24;
      }
      v11 = a1;
      if ( v2[70] != 1 )
        goto LABEL_23;
      http_io_fn_24(v2);
      v4 = 2;
      v2[13] = 2;
      v2[10] = 0;
      v2[70] = 0;
    }
  }
  v2[8] = sceNetInetGetErrno();
  v3 = -2143088541;
  v2[9] = sceNetInetGetPspError();
LABEL_44:
  v2[13] = 0;
LABEL_24:
  v12 = v2[12];
LABEL_25:
  if ( v12 > 0 )
  {
    result = v3;
    if ( v2[13] == 3 )
      return result;
    sceNetInetClose();
    v2[12] = 0;
    v2[13] = 0;
  }
  return v3;
}


int http_io_fn_2(int a1)
{
  int result;
  int v2;
  int v3; // dc
  int v4;
  int v5;
  int Tcpcbstat;
  int v7;
  int i;
  u16 v9[8];
  int v10;
  int v11[3];

  result = 0;
  if ( *(u32 *)(a1 + 64) != 0 )
  {
    v2 = *(u32 *)(a1 + 48);
    v10 = 16;
    v3 = sceNetInetGetsockname(v2, v9, &v10) < 0;
    result = 0;
    if ( !v3 )
    {
      v3 = sceNetInetGetTcpcbstat(v11, 0) < 0;
      result = 0;
      if ( !v3 )
      {
        if ( v11[0] != 0 && (v4 = http_opts_fn_2(v11[0]), v5 = v4, v4 != 0) )
        {
          Tcpcbstat = sceNetInetGetTcpcbstat(v11, v4);
          v7 = v5;
          if ( Tcpcbstat < 0 )
            goto LABEL_13;
          for ( i = *(u16 *)(v5 + 20); i != v9[1]; i = *(u16 *)(v7 + 20) )
          {
            v7 = *(u32 *)v7;
            if ( v7 == 0 )
              goto LABEL_13;
          }
          if ( v7 != 0 && *(u16 *)(v7 + 24) == 4 )
          {
            if ( v5 != 0 )
              http_opts_fn_3(v5);
            return 1;
          }
          else
          {
LABEL_13:
            if ( v5 != 0 )
              http_opts_fn_3(v5);
            return 0;
          }
        }
        else
        {
          return 0;
        }
      }
    }
  }
  return result;
}


int http_io_fn_3(int a1, int a2)
{
  int v4;
  int v5;
  int v6;

  v4 = http_io_fn_36(a1, a2);
  do
  {
    v5 = sceNet_lib_D8722983("\r\n");
    v6 = 0;
    if ( v4 == v5 )
      break;
    v4 = http_io_fn_36(a1, a2);
    v6 = v4;
  }
  while ( v4 >= 0 );
  return v6;
}


int http_io_fn_4(int a1)
{
  int Info;
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
  u32 v14[32];

  Info = sceNetApctlGetInfo(13, v14);
  if ( Info < 0 )
    return Info;
  if ( v14[0] != 1 )
  {
    *(u32 *)(a1 + 16) = 0;
    return Info;
  }
  *(u32 *)(a1 + 16) = 1;
  v4 = sceNetApctlGetInfo(15, v14);
  Info = v4;
  if ( v4 < 0 )
    return Info;
  *(u16 *)(a1 + 172) = v14[0];
  sceKernelMemset(v14, 0, 128);
  Info = sceNetApctlGetInfo(14, v14);
  if ( Info < 0 )
    return Info;
  http_io_fn_35(v14);
  v5 = http_io_fn_89();
  v6 = *(u32 *)(a1 + 64);
  v7 = v5;
  if ( v6 == 0 )
    goto LABEL_12;
  if ( sceNet_lib_A0F16ABD(v6, v14) != 0 )
  {
    v11 = *(u32 *)(a1 + 64);
    if ( v11 == 0 )
      goto LABEL_12;
    http_opts_fn_3(v11);
    v10 = 0;
    *(u32 *)(a1 + 64) = 0;
  }
  else
  {
    v10 = *(u32 *)(a1 + 64);
  }
  if ( v10 == 0 )
  {
LABEL_12:
    v12 = sceNet_lib_D8722983(v14);
    v13 = http_opts_fn_2(v12 + 1);
    *(u32 *)(a1 + 64) = v13;
    if ( v13 != 0 )
      sceNet_lib_80C9F02A(v13, v14);
    else
      Info = -2143088606;
  }
  http_io_fn_90(v7, v8, v9);
  return Info;
}


int http_io_fn_5(int a1, int a2, int a3, int a4)
{
  return http_io_fn_7(a1, a2, a3, a4, 1);
}


int http_io_fn_6(int a1, int a2, int a3, int a4)
{
  if ( *(u32 *)(a1 + 4) == 1 )
    return http_ssl_fn_15(a1);
  else
    return sceNetInetSend(*(u32 *)(a1 + 48), a2, a3, a4);
}


int http_io_fn_7(u32 *a1, int a2, unsigned int a3, int a4, int a5)
{
  unsigned int v7;
  unsigned int v8;
  int v9;
  int v10;
  int v11;
  u32 *v12;
  int *v13;
  unsigned int v14;
  unsigned int v15;
  unsigned int v16;
  unsigned int v17;
  int v18; // dc
  int v19;
  int v20;
  int v21;
  int PspError;
  int v23;
  int v24;
  int v25;
  int v27;
  u32 *v28;
  int v32;

  v7 = 0;
  v8 = 0;
  v9 = 0;
  v10 = a2;
  v32 = 0;
  if ( a1 != NULL )
  {
    v11 = a1[15];
    if ( v11 == 6 )
      return v32;
    if ( v11 == 7 )
      return -2143088541;
    v12 = (u32 *)a1[233];
    if ( v12 != NULL || (v12 = (u32 *)a1[232]) != NULL )
    {
      v13 = v12 + 92;
      if ( a5 == 1 && v12[93] < a3 )
        return -2143088380;
      while ( v7 != a3 )
      {
        if ( v9 == 1 )
          return v7;
        if ( a1[15] == 6 )
          return v7;
        v14 = v12[95];
        v15 = v12[94];
        if ( v14 == v15 )
        {
          if ( (unsigned int)(v12[13] - 3) >= 2 )
            return -2143088528;
          if ( a5 == 1 && v7 != 0 )
          {
            sceKernelMemcpy(*v13, a2, v7);
            v28 = v12;
            v19 = *v13 + v7;
            v27 = v12[93] - v7;
            v8 = v7;
          }
          else
          {
            v19 = *v13;
            v27 = v12[93];
            v28 = v12;
          }
          v20 = http_opts_fn_27(v28, v19, v27);
          if ( v20 == -2143088523 )
          {
            http_io_fn_42(v12, a1);
            http_io_fn_24(v12);
            v25 = 6;
LABEL_31:
            a1[15] = v25;
            return v32;
          }
          if ( v20 < 0 )
          {
            a1[9] = sceNetInetGetErrno();
            PspError = sceNetInetGetPspError();
            v23 = a1[9];
            a1[10] = PspError;
            v12[8] = v23;
            v24 = -2143088536;
            v12[9] = a1[10];
            if ( a1[9] != 0xB )
              v24 = -2143088541;
            v32 = v24;
            http_io_fn_24(v12);
            v25 = 7;
            goto LABEL_31;
          }
          v14 = v20 + v8;
          if ( v20 != 0 )
          {
            v21 = v12[93];
            v12[94] = v8;
            v15 = v8;
            v12[95] = v14;
            if ( v14 != v21 )
              v9 = 1;
          }
          else
          {
            v12[96] = 1;
            http_io_fn_24(v12);
            v14 = v12[95];
            v15 = v12[94];
            if ( (unsigned int)(a1[15] - 3) < 2 )
              a1[15] = 6;
          }
        }
        v16 = v14 - v15;
        v17 = a3 - v7;
        v18 = v16 < a3 - v7;
        v7 += v16;
        if ( !v18 )
        {
          v16 = v17;
          v7 = a3;
        }
        sceKernelMemcpy(v10, *v13 + v15, v16);
        v10 += v16;
        v12[94] += v16;
      }
      if ( a5 == 1 )
        v12[94] -= v7;
      return v7;
    }
  }
  return -2143088130;
}


int http_io_fn_8(int a1, int a2)
{
  int result;

  result = sceNetInetSetsockopt(a1, 0xFFFF, 4102, a2 + 32, 4);
  if ( result >= 0 )
  {
    result = sceNetInetSetsockopt(a1, 0xFFFF, 4101, a2 + 36, 4);
    if ( result >= 0 )
      return sceNetInetSetsockopt(a1, 6, 1, a2 + 92, 4);
  }
  return result;
}


int http_io_fn_9(int a1, int a2, unsigned int a3, u32 *a4, u32 *a5)
{
  int v9;
  int result;
  u32 *v11;
  int v12;
  u32 *v13;
  int v14; // dc
  unsigned int v15;
  int v16;
  u32 v17[8];
  u32 v18[8];
  u32 v19[4];
  int v20[4];

  v9 = sceNetInetConnect(a1, a2, 16);
  if ( v9 < 0 )
  {
    *a4 = ((int (*)(void))sceNetInetGetErrno)();
    *a5 = sceNetInetGetPspError();
    result = -2143088541;
    if ( *a4 != 119 )
      return result;
    *a4 = 0;
    *a5 = 0;
  }
  if ( v9 == 0 )
    return 0;
  sceKernelMemset(v17, 0, 32);
  v11 = &v17[a1 >> 5];
  v12 = 1 << (a1 & 0x1F);
  *v11 |= v12;
  v13 = NULL;
  if ( a3 != 0 )
    v13 = v19;
  qmemcpy(v18, v17, sizeof(v18));
  v19[1] = a3 % 0xF4240;
  v19[0] = a3 / 0xF4240;
  v14 = sceNetInetSelect(a1 + 1, v17, v18, 0, v13) == 0;
  result = -2143088536;
  if ( !v14 )
  {
    if ( (*v11 & v12) != 0 || (v11[8] & v12) != 0 )
    {
      v20[0] = 4;
      v15 = sceNetInetGetsockopt(a1, 0xFFFF, 4103, a5, v20);
      if ( __PAIR64__(*a5 != 0, v15 >> 31) != 0 )
      {
        if ( *a5 != 0 )
        {
          *a4 = (u16)*a5;
        }
        else
        {
          *a4 = sceNetInetGetErrno(v16);
          *a5 = sceNetInetGetPspError();
        }
        result = -2143088536;
        if ( *a4 != 0x74 )
          return -2143088541;
      }
      else
      {
        return 0;
      }
    }
    else
    {
      *a4 = sceNetInetGetErrno(a1);
      *a5 = sceNetInetGetPspError();
      return -2143088541;
    }
  }
  return result;
}


int http_io_fn_10(int a1, int a2, int a3, int a4, int a5, int a6)
{
  int v11;
  int v13;
  int v14;
  int v15;
  u8 v16[8];
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23[4];

  v23[0] = 0;
  sceKernelMemset(v16, 0, 44);
  v11 = -2143088639;
  if ( http_opts_fn_23() != 1 )
    return v11;
  v11 = sceUriParse(0, a3, 0, v23, 0);
  if ( v11 < 0 )
    return v11;
  v11 = -2143088606;
  v13 = http_opts_fn_2(v23[0]);
  if ( v13 == 0 )
    return v11;
  sceUriParse(v16, a3, v13, 0, v23[0]);
  v15 = http_io_fn_17(a1, a2, v20, v21, v22, v14, a5, a6, v17, v18, v19);
  http_opts_fn_3(v13);
  return v15;
}


int http_io_fn_11(int a1)
{
  char v2;
  u32 *v3;
  int v4;
  int v6;
  int v7;
  int v8;
  u32 *v9;
  u32 *v10;
  int v11;
  int v12;
  int v13;
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
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  u32 *v34;
  int j;
  int v36;
  int v37;
  int v38;
  int v39;
  u32 *v40;
  int i;
  int v42;
  u32 *v43;
  u32 v44[3];

  v2 = 0;
  v3 = NULL;
  v4 = -2143088639;
  if ( http_opts_fn_23() != 1 )
    return v4;
  v4 = -2143088384;
  if ( a1 == 0x2000000 )
    return v4;
  v8 = http_io_fn_89();
  v6 = http_io_fn_12(a1, &v43);
  v9 = v43;
  v4 = v6;
  if ( v43 != NULL )
  {
    v28 = v43[8];
    if ( v28 == 1 )
    {
      v29 = v43[239];
      if ( v29 != 0 )
        *(u32 *)(v29 + 952) = v43[238];
      v30 = v9[238];
      if ( v30 != 0 )
      {
        v31 = v9[239];
        v9 = v43;
        *(u32 *)(v30 + 956) = v31;
      }
      v3 = (u32 *)v9[233];
      if ( v3 == NULL )
        v3 = (u32 *)v9[232];
      v32 = v3[3];
      if ( v32 != 0 )
      {
        if ( v3[4] == v9[6] )
        {
          v3[3] = 0;
          v3[4] = 0;
          v3[5] = 0;
          v39 = v9[11];
          v2 = 1;
          if ( v39 != 0 )
          {
            http_io_fn_12(v39, v44);
            v40 = (u32 *)v44[0];
            if ( v44[0] != 0 )
            {
              for ( i = *(u32 *)(v44[0] + 32); ; i = *(u32 *)(v44[0] + 32) )
              {
                v42 = v40[11];
                v40[12] = 0;
                v40[8] = i - 1;
                v40[11] = 0;
                v40[15] = 8;
                http_io_fn_12(v42, v44);
                v40 = (u32 *)v44[0];
                if ( v44[0] == 0 )
                  break;
              }
            }
          }
        }
        else
        {
          if ( v9[12] != 0 )
          {
            v3[3] = v32 - 1;
            v3[5] = v9[12];
            http_io_fn_12(v9[12], v44);
            v38 = v44[0];
            v9 = v43;
            if ( v44[0] != 0 )
            {
              v7 = 1;
              --*(u32 *)(v44[0] + 32);
              *(u32 *)(v38 + 96) = 1;
              *(u32 *)(v38 + 44) = 0;
            }
          }
          v33 = v9[11];
          if ( v33 != 0 )
          {
            http_io_fn_12(v33, v44);
            v34 = (u32 *)v44[0];
            if ( v44[0] != 0 )
            {
              for ( j = v3[3]; ; j = v3[3] )
              {
                v34[15] = 8;
                v3[3] = j - 1;
                v36 = v34[8];
                v37 = v34[11];
                v34[12] = 0;
                v34[8] = v36 - 1;
                v34[11] = 0;
                http_io_fn_12(v37, v44);
                v34 = (u32 *)v44[0];
                if ( v44[0] == 0 )
                  break;
              }
            }
          }
        }
      }
      else
      {
        v9 = (u32 *)v9[15];
        if ( v9 != NULL && (unsigned int)v9 != 5 && v9 != (u32 *)8 )
          v2 = 1;
      }
    }
    else
    {
      v4 = -2143088537;
      v43[8] = v28 - 1;
    }
  }
  else
  {
    v7 = -2143092736;
    v4 = -2143088384;
  }
  http_io_fn_90(v8, v7, v9);
  if ( ((u8)v2 & (v3 != NULL)) != 0 )
    http_io_fn_24(v3);
  v10 = v43;
  if ( v4 < 0 )
    return v4;
  v11 = v43[28];
  if ( v11 != 0 )
  {
    http_opts_fn_3(v11);
    v10 = v43;
    v43[28] = 0;
  }
  v12 = v10[29];
  if ( v12 != 0 )
  {
    http_opts_fn_3(v12);
    v10 = v43;
    v43[29] = 0;
  }
  v13 = v10[30];
  if ( v13 != 0 )
  {
    http_opts_fn_3(v13);
    v10 = v43;
    v43[30] = 0;
  }
  v14 = v10[31];
  if ( v14 != 0 )
  {
    http_opts_fn_3(v14);
    v10 = v43;
  }
  v15 = v10[32];
  if ( v15 != 0 )
  {
    http_opts_fn_3(v15);
    v10 = v43;
  }
  v16 = v10[27];
  if ( v16 != 0 )
  {
    http_opts_fn_3(v16);
    v10 = v43;
  }
  v17 = v10[234];
  if ( v17 != 0 )
  {
    http_io_fn_64(v17);
    v10 = v43;
  }
  v18 = v10[235];
  if ( v18 != 0 )
  {
    http_io_fn_52(v18);
    v10 = v43;
  }
  v19 = (u32 *)v10[236];
  if ( v19 != NULL )
  {
    if ( v19[2] != 0 )
      http_cookie_fn_5((int)v19);
    else
      http_cookie_fn_6(v19);
    v10 = v43;
  }
  http_io_fn_30(v10 + 34);
  v20 = http_io_fn_89();
  --*(u32 *)(v43[232] + 8);
  http_io_fn_90(v20, v21, v22);
  v23 = (int)v43;
  if ( v43[233] != 0 )
  {
    v24 = http_io_fn_89();
    v25 = v43[233];
    v26 = *(u32 *)(v25 + 8) - 1;
    *(u32 *)(v25 + 8) = v26;
    http_io_fn_90(v24, v27, v26);
    v23 = (int)v43;
  }
  http_opts_fn_3(v23);
  return v4;
}


int http_io_fn_12(int a1, u32 *a2)
{
  int v2;
  u32 *v5;
  int v7;
  int v8;
  int v9;
  int i;

  v2 = 0;
  v5 = &HTTP_BSS_U8(0x2AD8);
  *a2 = 0;
  if ( a1 == 0 )
    return -2143088384;
  v7 = http_io_fn_89();
  if ( &HTTP_BSS_U8(0x2AD8) != NULL )
  {
    for ( i = HTTP_BSS_U32(0x2AF0); i != a1; i = v5[6] )
    {
      v9 = -2143092736;
      if ( a1 < i )
      {
        v2 = -2143088384;
        *a2 = 0;
        goto LABEL_10;
      }
      v5 = (u32 *)v5[238];
      if ( v5 == NULL )
        goto LABEL_10;
    }
    v8 = v5[8];
    v2 = 0;
    *a2 = v5;
    v5[8] = v8 + 1;
  }
LABEL_10:
  http_io_fn_90(v7, v8, v9);
  if ( v5 == NULL )
    return -2143088384;
  return v2;
}


int http_io_fn_13(u32 *a1)
{
  u32 *v1;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;

  v1 = &HTTP_BSS_U8(0x2AD8);
  v3 = http_io_fn_89();
  v4 = HTTP_BSS_U32(0x2E90);
  v5 = v3;
  if ( HTTP_BSS_U32(0x2E90) != 0 )
  {
    do
    {
      v6 = v1[6];
      v7 = *(u32 *)(v4 + 24);
      if ( v7 != v6 + 1 )
        break;
      v1 = (u32 *)v4;
      v4 = *(u32 *)(v4 + 952);
      v6 = v7;
    }
    while ( v4 != 0 );
  }
  else
  {
    v6 = HTTP_BSS_U32(0x2AF0);
  }
  v8 = -2143088544;
  if ( v6 <= 50331647 )
  {
    v9 = v1[238];
    v8 = v6 + 1;
    a1[6] = v6 + 1;
    a1[238] = v9;
    v10 = v1[238];
    a1[239] = v1;
    if ( v10 != 0 )
      *(u32 *)(v10 + 956) = a1;
    v1[238] = a1;
  }
  http_io_fn_90(v5, v6, -2143092736);
  return v8;
}


int http_io_fn_14(u32 *a1)
{
  int v2;
  char *v3;
  int v4;
  int v5;
  int v6;
  s16 v7;
  int v8; // dc
  int result;
  int v10;
  int v11;
  u32 v12[8];
  s16 v13;
  char v14[16];
  char v15[16];
  int v16[4];

  strcpy(v15, "https");
  strcpy(v14, "http");
  sceKernelMemset(v12, 0, 44);
  v2 = a1[233];
  if ( v2 == 0 )
    v2 = a1[232];
  v3 = v14;
  if ( *(u32 *)(v2 + 4) != 0 )
    v3 = v15;
  v12[1] = v3;
  v4 = a1[30];
  v5 = a1[28];
  v6 = a1[29];
  v7 = *(u16 *)(v2 + 400);
  v12[4] = *(u32 *)(v2 + 56);
  v13 = v7;
  v12[5] = v5;
  v12[6] = v6;
  v12[7] = v4;
  v8 = sceUriBuild(0, v16, 0, v12, 0xFFFF) < 0;
  result = 0;
  if ( !v8 )
  {
    v10 = http_opts_fn_2(v16[0]);
    v11 = v10;
    if ( v10 != 0 )
    {
      v8 = sceUriBuild(v10, 0, v16[0], v12, 0xFFFF) < 0;
      result = v11;
      if ( !v8 )
        return result;
      http_opts_fn_3(v11);
    }
    return 0;
  }
  return result;
}


int http_io_fn_15()
{
  int result;

  result = sceKernelMemset(&HTTP_BSS_U8(0x2AD8), 0, 960);
  HTTP_BSS_U32(0x2AF0) = 0x2000000;
  return result;
}


int http_io_fn_16(int a1, int a2, int a3, int a4, int a5, int a6)
{
  int v11;
  int result;

  v11 = http_opts_fn_23();
  result = -2143088639;
  if ( v11 == 1 )
    return http_io_fn_17(a1, a2, a3, 0, 0, -2143092736, a5, a6, 0, 0, 0);
  return result;
}


int http_io_fn_17(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11)
{
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
  int v27;
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
  int v38; // dc
  int v39;
  int v40;
  int v41;
  int v42;
  int i;
  int v44;
  int v45;
  int v46;
  int v47;
  int v48;
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
  int v62;
  int v63;
  int v64;

  v58 = a1;
  v63 = a10;
  v13 = -2143088606;
  v14 = 0;
  v15 = 0;
  v16 = 0;
  v17 = 0;
  v59 = a2;
  v60 = a5;
  v61 = a7;
  v62 = a8;
  v64 = a11;
  v55 = 0;
  v57 = 0;
  v56 = 0;
  v18 = sceNet_lib_D8722983(a3);
  v19 = http_opts_fn_2(v18 + 1);
  if ( v19 == 0 )
    goto LABEL_47;
  v20 = sceNet_lib_D8722983(a3);
  v13 = sceUriSweepPath(v19, a3, v20 + 1);
  if ( v13 < 0 )
    goto LABEL_47;
  v21 = v60;
  if ( a4 != 0 )
  {
    v22 = sceNet_lib_D8722983(a4);
    v23 = http_opts_fn_2(v22 + 1);
    v14 = v23;
    v13 = -2143088606;
    if ( v23 == 0 )
      goto LABEL_47;
    sceNet_lib_80C9F02A(v23, a4);
    v21 = v60;
  }
  if ( v21 != 0 )
  {
    v24 = sceNet_lib_D8722983(v21);
    v25 = http_opts_fn_2(v24 + 1);
    v15 = v25;
    v13 = -2143088606;
    if ( v25 == 0 )
      goto LABEL_47;
    sceNet_lib_80C9F02A(v25, v60);
  }
  v26 = v63;
  if ( a9 != 0 )
  {
    v27 = sceNet_lib_D8722983(a9);
    v28 = http_opts_fn_2(v27 + 1);
    v16 = v28;
    v13 = -2143088606;
    if ( v28 == 0 )
      goto LABEL_47;
    sceNet_lib_80C9F02A(v28, a9);
    v26 = v63;
  }
  if ( v26 == 0 )
  {
LABEL_15:
    v31 = http_opts_fn_2(960);
    v32 = (u32 *)v31;
    v13 = -2143088606;
    if ( v31 == 0 )
      goto LABEL_43;
    sceKernelMemset(v31, 0, 960);
    v34 = http_io_fn_89();
    v13 = http_io_fn_23(v58, &v55, v33);
    v37 = v55;
    if ( v55 != 0 )
    {
      v32[232] = v55;
      v13 = http_io_fn_33(v37 + 176, v32 + 34);
    }
    http_io_fn_90(v34, v35, v36);
    if ( v13 < 0 )
      goto LABEL_43;
    if ( v64 != 0 && (v38 = sceNet_lib_A0F16ABD(*(u32 *)(v55 + 56), v64) != 0, v39 = v55, v38) )
    {
      v41 = v64;
      v40 = *(u16 *)(v55 + 400);
    }
    else
    {
      v39 = v55;
      v40 = *(u16 *)(v55 + 400);
      v41 = *(u32 *)(v55 + 56);
    }
    v13 = http_io_fn_86(&v56, *(u32 *)(v39 + 4), v41, v40);
    if ( v13 < 0 || (v13 = http_io_fn_78(&v57, "Host", v56)) < 0 )
    {
LABEL_43:
      if ( v32 != NULL )
      {
        v50 = v32[31];
        if ( v50 != 0 )
          http_opts_fn_3(v50);
        http_io_fn_79(v32[72]);
        http_opts_fn_3((int)v32);
      }
      goto LABEL_47;
    }
    http_opts_fn_3(v56);
    v42 = v32[72];
    v56 = 0;
    if ( v42 != 0 )
    {
      for ( i = v42; ; i = v42 )
      {
        v42 = *(u32 *)(v42 + 8);
        if ( v42 == 0 )
          break;
      }
      *(u32 *)(i + 8) = v57;
    }
    else
    {
      v32[72] = v57;
    }
    v44 = v59;
    v32[15] = 0;
    v32[7] = v44;
    v32[28] = v19;
    v32[29] = v14;
    v32[30] = v15;
    if ( a9 == 0 || sceNet_lib_D8722983(a9) != 0 )
    {
      if ( v63 == 0 || sceNet_lib_D8722983(v63) != 0 )
        goto LABEL_40;
      v45 = v32[51];
    }
    else
    {
      v45 = v32[51];
    }
    if ( v45 != 0 )
      http_opts_fn_3(v45);
    v46 = v32[52];
    if ( v46 != 0 )
      http_opts_fn_3(v46);
    v32[51] = v16;
    v32[52] = v17;
LABEL_40:
    v47 = v62;
    *v32 = v61;
    v32[1] = v47;
    v48 = http_opts_fn_2(1500);
    v32[31] = v48;
    if ( v48 != 0 )
    {
      sceKernelMemset(v48, 0, 1500);
      v13 = http_io_fn_13(v32);
      if ( v13 >= 0 )
      {
        v32[27] = http_io_fn_14(v32);
        return v13;
      }
    }
    else
    {
      v52 = http_io_fn_89();
      v13 = -2143088606;
      --*(u32 *)(v55 + 8);
      http_io_fn_90(v52, v53, v54);
    }
    goto LABEL_43;
  }
  v29 = sceNet_lib_D8722983(v26);
  v30 = http_opts_fn_2(v29 + 1);
  v17 = v30;
  v13 = -2143088606;
  if ( v30 != 0 )
  {
    sceNet_lib_80C9F02A(v30, v63);
    goto LABEL_15;
  }
LABEL_47:
  if ( v19 != 0 )
    http_opts_fn_3(v19);
  if ( v14 != 0 )
    http_opts_fn_3(v14);
  if ( v15 != 0 )
    http_opts_fn_3(v15);
  if ( v16 != 0 )
    http_opts_fn_3(v16);
  v51 = v56;
  if ( v17 != 0 )
  {
    http_opts_fn_3(v17);
    v51 = v56;
  }
  if ( v51 != 0 )
    http_opts_fn_3(v51);
  return v13;
}


int http_io_fn_18(int a1, int a2, int a3, int a4, int a5, unsigned int a6, int a7, int a8, int a9)
{
  int v16;
  int v17;
  int result;

  v16 = a6;
  v17 = http_opts_fn_23();
  result = -2143088639;
  if ( v17 == 1 )
    return http_io_fn_20(a1, a2, a3, a4, a5, v16, a7, a8, a9);
  return result;
}


int http_io_fn_19(int a1, int a2, int a3, int a4, int a5)
{
  int v10;
  int v11;
  int v12;
  u8 v14[4];
  int v15;
  int v16;
  int v17;
  int v18;
  u16 v19;
  int v20[4];

  v20[0] = 0;
  sceKernelMemset(v14, 0, 44);
  v10 = sceUriParse(0, a2, 0, v20, 0);
  if ( v10 >= 0 )
  {
    v10 = -2143088606;
    v11 = http_opts_fn_2(v20[0]);
    if ( v11 == 0
      || (sceUriParse(v14, a2, v11, 0, v20[0]),
          v10 = http_io_fn_20(a1, v18, v15, v16, v17, v19, a3, a4, a5),
          v12 = v11,
          v10 < 0) )
    {
      v12 = v11;
      if ( v11 == 0 )
        return v10;
    }
    http_opts_fn_3(v12);
  }
  return v10;
}


int http_io_fn_20(int a1, int a2, int a3, int a4, int a5, unsigned int a6, int a7, int a8, int a9)
{
  int v11;
  int v12;
  s64 v13;
  int v14;
  int v15;
  unsigned int v16;
  int v17; // dc
  int result;
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
  int v46;
  int v47;
  int v48;
  int v49;

  v45 = a1;
  v11 = a6;
  v12 = 0;
  v13 = 0;
  v46 = a4;
  v47 = a5;
  v48 = a7;
  v49 = a9;
  v14 = http_io_fn_37(a3);
  v15 = v14;
  if ( v14 < 0 )
    return v15;
  v16 = a8 - 1;
  if ( v16 < 2 )
  {
    v17 = http_io_fn_26(v49, a2, v14, v11) < 0;
    result = 0;
    if ( !v17 )
      return result;
  }
  v19 = http_opts_fn_2(404);
  v20 = v19;
  if ( v19 == 0 )
    goto LABEL_21;
  sceKernelMemset(v19, 0, 404);
  if ( a2 == 0 )
  {
    v15 = -2143088130;
    goto LABEL_22;
  }
  v21 = sceNet_lib_D8722983(a2) + 1;
  v22 = http_opts_fn_2(v21);
  v12 = v22;
  if ( v22 == 0 )
    goto LABEL_21;
  sceKernelMemcpy(v22, a2, v21);
  *(u8 *)(v12 + v21 - 1) = 0;
  *(u16 *)(v20 + 400) = v11;
  *(u32 *)(v20 + 4) = v15;
  *(u32 *)(v20 + 56) = v12;
  *(u32 *)(v20 + 52) = 0;
  if ( v16 < 2 )
  {
    v15 = http_io_fn_33(*(u32 *)(v49 + 928) + 176, v20 + 176);
    goto LABEL_19;
  }
  if ( v46 != 0 )
  {
    v24 = sceNet_lib_D8722983(v46) + 1;
    v25 = http_opts_fn_2(v24);
    v13 = ((u64)(v13) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v25) << 32);
    v15 = -2143088606;
    if ( v25 == 0 )
      goto LABEL_22;
    sceKernelMemcpy(v25, v46, v24);
    v23 = v47;
  }
  else
  {
    v23 = v47;
  }
  if ( v23 != 0 )
  {
    v26 = sceNet_lib_D8722983(v23) + 1;
    v27 = http_opts_fn_2(v26);
    v13 = ((u64)(v13) & 0xFFFFFFFF00000000ULL) | (u32)(v27);
    v15 = -2143088606;
    if ( v27 != 0 )
    {
      sceKernelMemcpy(v27, v47, v26);
      goto LABEL_16;
    }
LABEL_22:
    if ( v20 != 0 )
    {
      v33 = *(u32 *)(v20 + 368);
      if ( v33 != 0 )
        http_opts_fn_3(v33);
      http_opts_fn_3(v20);
    }
    if ( v12 != 0 )
      http_opts_fn_3(v12);
    if ( HIDWORD(v13) != 0 )
      http_opts_fn_3(SHIDWORD(v13));
    if ( (u32)v13 == 0 )
      return v15;
    http_opts_fn_3(v13);
    return v15;
  }
LABEL_16:
  v29 = http_io_fn_89();
  v15 = http_io_fn_31(v45, &v44, v28);
  if ( v44 != 0 )
  {
    v15 = http_io_fn_33(v44, v20 + 176);
    --*(u32 *)(v44 + 8);
  }
  http_io_fn_90(v29, v30, v31);
LABEL_19:
  if ( v15 < 0 )
    goto LABEL_22;
  v32 = http_opts_fn_2(*(u32 *)(v20 + 220));
  *(u32 *)(v20 + 368) = v32;
  if ( v32 == 0 )
  {
LABEL_21:
    v15 = -2143088606;
    goto LABEL_22;
  }
  *(u32 *)(v20 + 372) = *(u32 *)(v20 + 220);
  sceKernelMemset(v32, 0, *(u32 *)(v20 + 372));
  v34 = v48;
  if ( v13 != 0 )
  {
    v35 = *(u32 *)(v20 + 244);
    if ( v35 != 0 )
      http_opts_fn_3(v35);
    v36 = *(u32 *)(v20 + 248);
    if ( v36 != 0 )
      http_opts_fn_3(v36);
    *(u32 *)(v20 + 244) = HIDWORD(v13);
    *(u32 *)(v20 + 248) = v13;
    v34 = v48;
  }
  *(u32 *)(v20 + 252) = v34;
  if ( v16 >= 2 )
  {
    v15 = http_io_fn_22(v20);
    if ( v15 < 0 )
      goto LABEL_22;
    return v15;
  }
  v37 = http_io_fn_89();
  v40 = *(u32 *)(v49 + 928);
  v41 = *(u32 *)(v40 + 388);
  if ( v41 != 0 )
  {
    *(u32 *)(v20 + 388) = v41;
    v40 = *(u32 *)(v49 + 928);
  }
  v42 = *(u32 *)(v20 + 8);
  v43 = v49;
  *(u32 *)(v40 + 388) = v20;
  *(u32 *)(v20 + 8) = v42 + 1;
  *(u32 *)(v43 + 932) = v20;
  http_io_fn_90(v37, v38, v39);
  return v15;
}


int http_io_fn_21(int a1)
{
  int v2;
  int result;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  u32 *v9;
  int v10;
  u32 *v11;
  int i;
  int v13;
  int v14;
  int v15[4];

  v2 = -2143088639;
  if ( http_opts_fn_23() != 1 )
    return v2;
  v2 = -2143088384;
  if ( a1 == 0x1000000 )
    return v2;
  v8 = http_io_fn_89();
  v5 = http_io_fn_23(a1, v15, v4);
  v9 = (u32 *)v15[0];
  v2 = v5;
  if ( v15[0] != 0 )
  {
    v10 = *(u32 *)(v15[0] + 8);
    v7 = -2143092736;
    if ( v10 == 1 )
    {
      v13 = *(u32 *)(v15[0] + 396);
      if ( v13 != 0 )
        *(u32 *)(v13 + 392) = *(u32 *)(v15[0] + 392);
      v14 = v9[98];
      if ( v14 != 0 )
        *(u32 *)(v14 + 396) = v9[99];
    }
    else
    {
      v6 = v10 - 1;
      v2 = -2143088537;
      *(u32 *)(v15[0] + 8) = v10 - 1;
    }
  }
  else
  {
    v2 = -2143088384;
  }
  http_io_fn_90(v8, v6, v7);
  v11 = (u32 *)v15[0];
  if ( v2 < 0 )
    return v2;
  result = v2;
  if ( v15[0] != 0 )
  {
    for ( i = *(u32 *)(v15[0] + 56); ; i = v11[14] )
    {
      v15[0] = (int)v11;
      if ( i != 0 )
      {
        http_opts_fn_3(i);
        v11 = (u32 *)v15[0];
        *(u32 *)(v15[0] + 56) = 0;
      }
      http_io_fn_30(v11 + 44);
      http_io_fn_24(v15[0]);
      http_opts_fn_3(*(u32 *)(v15[0] + 368));
      v11 = *(u32 **)(v15[0] + 388);
      http_opts_fn_3(v15[0]);
      if ( v11 == NULL )
        break;
    }
    return v2;
  }
  return result;
}


int http_io_fn_22(u32 *a1)
{
  int *v1;
  int v3;
  int *v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;

  v1 = &HTTP_BSS_U32(0x2E98);
  v3 = http_io_fn_89();
  v4 = (int *)HTTP_BSS_U32(0x3020);
  v5 = v3;
  if ( HTTP_BSS_U32(0x3020) != 0 )
  {
    do
    {
      v6 = *v1;
      v7 = *v4;
      if ( *v4 != *v1 + 1 )
        break;
      v1 = v4;
      v4 = (int *)v4[98];
      v6 = v7;
    }
    while ( v4 != NULL );
  }
  else
  {
    v6 = HTTP_BSS_U32(0x2E98);
  }
  v8 = -2143088544;
  if ( v6 <= 0x1FFFFFF )
  {
    v9 = v1[98];
    v8 = v6 + 1;
    *a1 = v6 + 1;
    a1[98] = v9;
    v10 = v1[98];
    a1[99] = v1;
    if ( v10 != 0 )
      *(u32 *)(v10 + 396) = a1;
    v1[98] = (int)a1;
  }
  http_io_fn_90(v5, v6, -2143092736);
  return v8;
}


int http_io_fn_23(int a1, int **a2)
{
  int v2;
  int *v5;
  int v7;
  int v8;
  int v9;
  int i;

  v2 = 0;
  v5 = &HTTP_BSS_U32(0x2E98);
  *a2 = NULL;
  if ( a1 == 0 )
    return -2143088384;
  v7 = http_io_fn_89();
  if ( &HTTP_BSS_U32(0x2E98) != NULL )
  {
    for ( i = HTTP_BSS_U32(0x2E98); i != a1; i = *v5 )
    {
      v9 = -2143092736;
      if ( a1 < i )
      {
        v2 = -2143088384;
        *a2 = NULL;
        goto LABEL_10;
      }
      v5 = (int *)v5[98];
      if ( v5 == NULL )
        goto LABEL_10;
    }
    v8 = v5[2];
    v2 = 0;
    *a2 = v5;
    v5[2] = v8 + 1;
  }
LABEL_10:
  http_io_fn_90(v7, v8, v9);
  if ( v5 == NULL )
    return -2143088384;
  return v2;
}


int http_io_fn_24(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int result;
  u32 *v9;
  int i;
  u32 v11[4];

  v4 = http_io_fn_89();
  v5 = a1[3];
  a1[13] = 7;
  if ( v5 >= 2 )
  {
    http_io_fn_12(a1[4], v11);
    v9 = (u32 *)v11[0];
    if ( v11[0] != 0 )
    {
      for ( i = *(u32 *)(v11[0] + 32); ; i = *(u32 *)(v11[0] + 32) )
      {
        v9[12] = 0;
        v9[8] = i - 1;
        v9[15] = 8;
        http_io_fn_12(v9[11], v11);
        v9 = (u32 *)v11[0];
        if ( v11[0] == 0 )
          break;
      }
    }
  }
  http_io_fn_90(v4, v2, v3);
  if ( a1[1] == 1 )
    http_ssl_fn_17((int)a1, 1);
  if ( (int)a1[12] > 0 )
    sceNetInetClose();
  v6 = a1[92];
  v7 = a1[93];
  a1[12] = 0;
  result = sceKernelMemset(v6, 0, v7);
  a1[13] = 0;
  a1[94] = 0;
  a1[95] = 0;
  a1[96] = 0;
  a1[3] = 0;
  a1[18] = 0;
  return result;
}


int http_io_fn_25()
{
  int result;

  result = sceKernelMemset(&HTTP_BSS_U32(0x2E98), 0, 404);
  HTTP_BSS_U32(0x2E98) = 0x1000000;
  return result;
}


int http_io_fn_26(int a1, int a2, int a3, unsigned int a4)
{
  int v6;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int result;

  v6 = 0;
  v8 = *(u32 *)(a1 + 928);
  v9 = a4;
  if ( v8 == 0 )
    return -2143088603;
  v10 = *(u32 *)(v8 + 388);
  v13 = http_io_fn_89();
  while ( v10 != 0 )
  {
    if ( a2 == 0 )
      break;
    v11 = *(u32 *)(v10 + 56);
    if ( v11 == 0 )
      break;
    if ( sceNet_lib_9CFBC7E3(a2, v11) != 0 )
    {
      v10 = *(u32 *)(v10 + 388);
    }
    else if ( a3 == *(u32 *)(v10 + 4) )
    {
      if ( *(u16 *)(v10 + 400) == v9 )
      {
        v12 = *(u32 *)(v10 + 8);
        v6 = 1;
        *(u32 *)(a1 + 932) = v10;
        v11 = v12 + 1;
        *(u32 *)(v10 + 8) = v12 + 1;
        break;
      }
      v10 = *(u32 *)(v10 + 388);
    }
    else
    {
      v10 = *(u32 *)(v10 + 388);
    }
  }
  http_io_fn_90(v13, v11, v12);
  result = 0;
  if ( v6 != 1 )
    return -2143088603;
  return result;
}


// write access to const memory has been detected, the output may be wrong!
int http_io_fn_27(int a1, unsigned int a2, int a3)
{
  int v4;
  int v8;
  int v9;
  int CompiledSdkVersion;
  int v11;
  int v12;
  int v13;
  int i;
  u32 v15[4];

  v4 = 0;
  v15[0] = 0;
  if ( http_opts_fn_23() != 1 )
    return -2143088639;
  v8 = http_opts_fn_2(176);
  v9 = -2143088606;
  if ( v8 == 0 )
    goto LABEL_27;
  sceKernelMemset(v8, 0, 176);
  CompiledSdkVersion = sceKernelGetCompiledSdkVersion();
  if ( a1 != 0 )
  {
    v9 = http_io_fn_78(v15, "User-Agent", a1);
    if ( v9 < 0 )
      goto LABEL_27;
    if ( *(u32 *)(v8 + 152) != 0 )
    {
      *(volatile u32 *)8 = v15[0];
      v4 = v15[0];
    }
    else
    {
      v4 = v15[0];
      *(u32 *)(v8 + 152) = v15[0];
    }
  }
  v9 = http_io_fn_78(v15, "Accept", "*/*;q=0.01");
  if ( v9 >= 0 )
  {
    v11 = v15[0];
    if ( *(u32 *)(v8 + 152) != 0 )
      *(u32 *)(v4 + 8) = v15[0];
    else
      *(u32 *)(v8 + 152) = v15[0];
    v9 = http_io_fn_78(v15, "Accept-Encoding", "");
    if ( v9 >= 0 )
    {
      v12 = v15[0];
      if ( *(u32 *)(v8 + 152) != 0 )
        *(u32 *)(v11 + 8) = v15[0];
      else
        *(u32 *)(v8 + 152) = v15[0];
      v9 = http_io_fn_78(v15, "Accept-Charset", "iso-8859-1;q=0.01");
      if ( v9 >= 0 )
      {
        if ( *(u32 *)(v8 + 152) != 0 )
          *(u32 *)(v12 + 8) = v15[0];
        else
          *(u32 *)(v8 + 152) = v15[0];
        v13 = *(u32 *)(v8 + 152);
        if ( v13 != 0 )
        {
          for ( i = *(u32 *)(v13 + 8); i != 0; i = *(u32 *)(i + 8) )
            ;
        }
        v9 = -2143088534;
        if ( a2 < 2 )
        {
          *(u32 *)(v8 + 4) = a2;
          *(u16 *)(v8 + 96) = a3;
          *(u32 *)(v8 + 24) = 3000000;
          *(u32 *)(v8 + 12) = 10;
          *(u32 *)(v8 + 40) = 5000;
          *(u32 *)(v8 + 44) = 1500;
          *(u32 *)(v8 + 60) = 6;
          *(u32 *)(v8 + 56) = 6;
          *(u32 *)(v8 + 80) = 1;
          *(u32 *)(v8 + 20) = 0;
          *(u32 *)(v8 + 84) = 1;
          *(u32 *)(v8 + 88) = 1;
          if ( CompiledSdkVersion > 101056767 )
            *(u32 *)(v8 + 104) = 2;
          else
            *(u32 *)(v8 + 104) = 1;
          return http_io_fn_32(v8);
        }
      }
    }
  }
LABEL_27:
  if ( v8 != 0 )
  {
    http_io_fn_79(*(u32 *)(v8 + 152));
    http_opts_fn_3(v8);
  }
  return v9;
}


int http_io_fn_28(int a1, int a2, int a3)
{
  int v3;
  int v4;
  int result;
  int v6;
  int v7;
  int v8;
  u32 v9[4];

  v3 = 0;
  v9[0] = 0;
  v4 = http_io_fn_31(a1, v9, a3);
  if ( v4 >= 0 )
  {
    v3 = http_opts_fn_2(176);
    v4 = -2143088606;
    if ( v3 != 0 )
    {
      sceKernelMemset(v3, 0, 176);
      v4 = http_io_fn_33(v9[0], v3);
      if ( v4 >= 0 )
        v4 = http_io_fn_32(v3);
    }
  }
  if ( v9[0] != 0 )
  {
    v6 = http_io_fn_89();
    v7 = v9[0];
    v8 = *(u32 *)(v9[0] + 8) - 1;
    *(u32 *)(v9[0] + 8) = v8;
    http_io_fn_90(v6, v7, v8);
  }
  if ( (((unsigned int)v4 >> 31) & (v3 != 0)) != 0 )
    http_opts_fn_3(v3);
  result = v4;
  if ( v4 >= 0 )
    return *(u32 *)v3;
  return result;
}


int http_io_fn_29(int a1)
{
  int v2;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  u32 *v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14[4];

  v2 = -2143088639;
  if ( http_opts_fn_23() != 1 )
    return v2;
  v2 = -2143088384;
  if ( a1 == 0 )
    return v2;
  v8 = http_io_fn_89();
  v5 = http_io_fn_31(a1, v14, v4);
  v9 = (u32 *)v14[0];
  v10 = v5;
  if ( v14[0] != 0 )
  {
    v11 = *(u32 *)(v14[0] + 8);
    v7 = -2143092736;
    if ( v11 == 1 )
    {
      v12 = *(u32 *)(v14[0] + 168);
      if ( v12 != 0 )
        *(u32 *)(v12 + 164) = *(u32 *)(v14[0] + 164);
      v13 = v9[41];
      if ( v13 != 0 )
        *(u32 *)(v13 + 168) = v9[42];
    }
    else
    {
      v6 = v11 - 1;
      v10 = -2143088537;
      *(u32 *)(v14[0] + 8) = v11 - 1;
    }
  }
  else
  {
    v10 = -2143088384;
  }
  http_io_fn_90(v8, v6, v7);
  if ( v10 >= 0 )
  {
    http_io_fn_30(v14[0]);
    http_opts_fn_3(v14[0]);
  }
  return v10;
}


int http_io_fn_30(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int result;

  v2 = a1[38];
  if ( v2 != 0 )
  {
    result = http_io_fn_79(v2);
    a1[38] = 0;
  }
  v3 = a1[16];
  if ( v3 != 0 )
  {
    result = http_opts_fn_3(v3);
    a1[16] = 0;
  }
  v4 = a1[17];
  if ( v4 != 0 )
  {
    result = http_opts_fn_3(v4);
    a1[17] = 0;
  }
  v5 = a1[18];
  if ( v5 != 0 )
  {
    result = http_opts_fn_3(v5);
    a1[18] = 0;
  }
  return result;
}


int http_io_fn_31(int a1, int **a2)
{
  int *v4;
  int v5;
  int result;
  int v7;
  int v8;
  int i;

  v4 = &HTTP_BSS_U32(0x302C);
  v5 = 0;
  *a2 = NULL;
  if ( a1 == 0 )
    return -2143088384;
  v7 = http_io_fn_89();
  if ( &HTTP_BSS_U32(0x302C) != NULL )
  {
    for ( i = HTTP_BSS_U32(0x302C); i != a1; i = *v4 )
    {
      if ( a1 < i )
      {
        v5 = -2143088384;
        *a2 = NULL;
        goto LABEL_11;
      }
      v4 = (int *)v4[41];
      if ( v4 == NULL )
        goto LABEL_11;
    }
    v8 = v4[2];
    v5 = 0;
    *a2 = v4;
    v4[2] = v8 + 1;
  }
LABEL_11:
  http_io_fn_90(v7, v8, i);
  result = v5;
  if ( v4 == NULL )
  {
    *a2 = NULL;
    return -2143088384;
  }
  return result;
}


int http_io_fn_32(u32 *a1)
{
  int *v1;
  int v3;
  int *v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;

  v1 = &HTTP_BSS_U32(0x302C);
  v3 = http_io_fn_89();
  v4 = (int *)HTTP_BSS_U32(0x30D0);
  v5 = v3;
  if ( HTTP_BSS_U32(0x30D0) != 0 )
  {
    do
    {
      v6 = *v1;
      v7 = *v4;
      if ( *v4 != *v1 + 1 )
        break;
      v1 = v4;
      v4 = (int *)v4[41];
      v6 = v7;
    }
    while ( v4 != NULL );
  }
  else
  {
    v6 = HTTP_BSS_U32(0x302C);
  }
  v8 = -2143088544;
  if ( v6 <= 0xFFFFFF )
  {
    v9 = v1[41];
    v8 = v6 + 1;
    *a1 = v6 + 1;
    a1[41] = v9;
    v10 = v1[41];
    a1[42] = v1;
    if ( v10 != 0 )
      *(u32 *)(v10 + 168) = a1;
    v1[41] = (int)a1;
  }
  http_io_fn_90(v5, v6, -2143092736);
  return v8;
}


int http_io_fn_33(int a1, int a2)
{
  int v4;
  int v5;
  s16 v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  s16 v14;
  s16 v15;
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
  int result;
  int v42;
  int v43;
  int v44;
  int v45;
  int v46;
  int v47;
  int v48;
  int v49;
  int v50;

  v4 = -2143088130;
  if ( a2 == 0 || a1 == 0 )
    goto LABEL_12;
  sceKernelMemset(a2, 0, 176);
  v5 = *(u32 *)(a1 + 4);
  v6 = *(u16 *)(a1 + 96);
  v7 = *(u32 *)(a1 + 24);
  v8 = *(u32 *)(a1 + 12);
  v9 = *(u32 *)(a1 + 76);
  v10 = *(u32 *)(a1 + 80);
  v11 = *(u32 *)(a1 + 20);
  v12 = *(u32 *)(a1 + 84);
  v13 = *(u32 *)(a1 + 88);
  v14 = *(u16 *)(a1 + 174);
  v15 = *(u16 *)(a1 + 98);
  v16 = *(u32 *)(a1 + 100);
  v17 = *(u32 *)(a1 + 104);
  v18 = *(u32 *)(a1 + 28);
  v19 = *(u32 *)(a1 + 32);
  *(u32 *)a2 = *(u32 *)a1;
  *(u32 *)(a2 + 4) = v5;
  *(u16 *)(a2 + 96) = v6;
  *(u32 *)(a2 + 24) = v7;
  *(u32 *)(a2 + 12) = v8;
  *(u32 *)(a2 + 76) = v9;
  *(u32 *)(a2 + 80) = v10;
  *(u32 *)(a2 + 20) = v11;
  *(u32 *)(a2 + 84) = v12;
  *(u32 *)(a2 + 88) = v13;
  *(u16 *)(a2 + 174) = v14;
  *(u16 *)(a2 + 98) = v15;
  *(u32 *)(a2 + 100) = v16;
  *(u32 *)(a2 + 104) = v17;
  *(u32 *)(a2 + 28) = v18;
  *(u32 *)(a2 + 156) = 0;
  *(u32 *)(a2 + 160) = 0;
  *(u32 *)(a2 + 32) = v19;
  v20 = *(u32 *)(a1 + 136);
  v21 = *(u32 *)(a1 + 140);
  v22 = *(u32 *)(a1 + 132);
  v23 = *(u32 *)(a1 + 36);
  v24 = *(u32 *)(a1 + 40);
  v25 = *(u32 *)(a1 + 44);
  v26 = *(u32 *)(a1 + 48);
  v27 = *(u32 *)(a1 + 56);
  v28 = *(u32 *)(a1 + 60);
  v29 = *(u32 *)(a1 + 16);
  v9 = ((v9) & ~0xFFFFU) | ((u16)(*(u16 *)(a1 + 172)));
  v30 = *(u32 *)(a1 + 108);
  v31 = *(u32 *)(a1 + 112);
  v32 = *(u32 *)(a1 + 128);
  v33 = *(u32 *)(a1 + 120);
  v34 = *(u32 *)(a1 + 124);
  *(u32 *)(a2 + 116) = *(u32 *)(a1 + 116);
  *(u32 *)(a2 + 136) = v20;
  *(u32 *)(a2 + 140) = v21;
  *(u32 *)(a2 + 132) = v22;
  *(u32 *)(a2 + 36) = v23;
  *(u32 *)(a2 + 40) = v24;
  *(u32 *)(a2 + 44) = v25;
  *(u32 *)(a2 + 48) = v26;
  *(u32 *)(a2 + 56) = v27;
  *(u32 *)(a2 + 60) = v28;
  *(u32 *)(a2 + 16) = v29;
  *(u16 *)(a2 + 172) = v9;
  *(u32 *)(a2 + 108) = v30;
  *(u32 *)(a2 + 112) = v31;
  *(u32 *)(a2 + 128) = v32;
  *(u32 *)(a2 + 120) = v33;
  *(u32 *)(a2 + 124) = v34;
  v35 = *(u32 *)(a1 + 148);
  v36 = *(u32 *)(a1 + 92);
  v37 = *(u32 *)(a1 + 64);
  *(u32 *)(a2 + 144) = *(u32 *)(a1 + 144);
  *(u32 *)(a2 + 148) = v35;
  *(u32 *)(a2 + 92) = v36;
  if ( v37 != 0 )
  {
    v49 = sceNet_lib_D8722983(v37);
    v50 = http_opts_fn_2(v49 + 1);
    *(u32 *)(a2 + 64) = v50;
    v4 = -2143088606;
    if ( v50 == 0 )
      goto LABEL_12;
    sceNet_lib_80C9F02A(v50, *(u32 *)(a1 + 64));
    v38 = *(u32 *)(a1 + 68);
  }
  else
  {
    v38 = *(u32 *)(a1 + 68);
  }
  if ( v38 != 0 )
  {
    if ( sceNet_lib_D8722983(v38) != 0 )
    {
      v4 = -2143088606;
      v47 = sceNet_lib_D8722983(*(u32 *)(a1 + 68));
      v48 = http_opts_fn_2(v47 + 1);
      *(u32 *)(a2 + 68) = v48;
      if ( v48 == 0 )
        goto LABEL_12;
      sceNet_lib_80C9F02A(v48, *(u32 *)(a1 + 68));
      v39 = *(u32 *)(a1 + 72);
    }
    else
    {
      v39 = *(u32 *)(a1 + 72);
    }
  }
  else
  {
    v39 = *(u32 *)(a1 + 72);
  }
  if ( v39 == 0 )
  {
    v40 = *(u32 *)(a1 + 152);
    goto LABEL_11;
  }
  if ( sceNet_lib_D8722983(v39) == 0 )
  {
    v40 = *(u32 *)(a1 + 152);
    goto LABEL_11;
  }
  v4 = -2143088606;
  v45 = sceNet_lib_D8722983(*(u32 *)(a1 + 72));
  v46 = http_opts_fn_2(v45 + 1);
  *(u32 *)(a2 + 72) = v46;
  if ( v46 != 0 )
  {
    sceNet_lib_80C9F02A(v46, *(u32 *)(a1 + 72));
    v40 = *(u32 *)(a1 + 152);
LABEL_11:
    v4 = http_io_fn_81(a2 + 152, v40);
    result = 0;
    if ( v4 >= 0 )
      return result;
  }
LABEL_12:
  http_io_fn_79(*(u32 *)(a2 + 152));
  v42 = *(u32 *)(a2 + 64);
  if ( v42 != 0 )
    http_opts_fn_3(v42);
  v43 = *(u32 *)(a2 + 68);
  if ( v43 != 0 )
    http_opts_fn_3(v43);
  v44 = *(u32 *)(a2 + 72);
  if ( v44 != 0 )
    http_opts_fn_3(v44);
  return v4;
}


int http_io_fn_34()
{
  int result;

  result = sceKernelMemset(&HTTP_BSS_U32(0x302C), 0, 176);
  HTTP_BSS_U32(0x302C) = 0;
  return result;
}


void http_io_fn_35(int a1)
{
  int v2;
  int v3;
  u32 v4[12];
  int v5[4];

  if ( a1 != 0 && sceUriParse(0, a1, 0, v5, 0) >= 0 )
  {
    v2 = http_opts_fn_2(v5[0]);
    if ( v2 != 0 )
    {
      if ( sceUriParse(v4, a1, v2, 0, v5[0]) >= 0 )
      {
        v4[0] = 1;
        if ( sceUriBuild(0, v5, 0, v4, 2) >= 0 )
        {
          v3 = sceNet_lib_D8722983(a1);
          if ( (unsigned int)(v3 + 1) >= v5[0] )
            sceUriBuild(a1, 0, v5[0], v4, 2);
        }
      }
      http_opts_fn_3(v2);
    }
  }
}


int http_io_fn_36(int a1, int a2)
{
  int v2;
  int v4;
  int v5;
  u32 *v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u8 v12[16];

  v2 = a1;
  v4 = 0;
  v5 = 0;
  v6 = *(u32 **)(a1 + 932);
  v7 = false;
  if ( v6 == NULL )
    v6 = *(u32 **)(a1 + 928);
  while ( 1 )
  {
    v8 = http_opts_fn_25(a1, (int)v12, 1, a2);
    v9 = v8;
    if ( v8 < 0 )
      return v9;
    if ( v8 == 0 )
    {
      v9 = -2143088540;
      http_io_fn_24(v6);
      return v9;
    }
    ++v5;
    if ( v7 )
    {
      v10 = v12[0];
      if ( v12[0] == 10 )
      {
        v4 = 1;
        goto LABEL_8;
      }
    }
    else
    {
      v10 = v12[0];
    }
    v7 = v10 == 0xD;
LABEL_8:
    a1 = v2;
    if ( v4 == 1 )
      return v5;
  }
}


int http_io_fn_37(int a1)
{
  int result;
  int v3;

  result = -2143088130;
  v3 = 0;
  if ( a1 != 0 )
  {
    if ( sceNet_lib_9CFBC7E3(a1, "HTTP") != 0 )
    {
      v3 = 1;
      if ( sceNet_lib_9CFBC7E3(a1, "HTTPS") != 0 )
        return -2143088543;
    }
    return v3;
  }
  return result;
}


int http_io_fn_38(int a1)
{
  u32 *v1;
  u32 *v2;
  int i;
  int v4;
  int v5;
  u64 v7;
  int v8;
  int v9;
  u8 v10[128];

  v1 = *(u32 **)(a1 + 932);
  v2 = (u32 *)a1;
  if ( v1 == NULL )
    v1 = *(u32 **)(a1 + 928);
  if ( *(u32 *)(a1 + 28) == 2 )
  {
LABEL_14:
    v5 = v2[212];
  }
  else if ( *(u32 *)(a1 + 840) == 1 )
  {
    v7 = *(u64 *)(a1 + 832) - *(u64 *)(a1 + 16);
    if ( *(u64 *)(a1 + 832) == *(u64 *)(a1 + 16) )
    {
      v5 = *(u32 *)(a1 + 848);
    }
    else
    {
      do
      {
        v8 = 128;
        if ( v7 < 0x80 )
        {
          a1 = (int)v2;
          v8 = v7;
        }
        v9 = http_opts_fn_25(a1, (int)v10, v8, 0);
        if ( v9 < 0 )
        {
          v2[22] = 0;
          return 0;
        }
        if ( v9 == 0 )
          goto LABEL_14;
        v7 -= v9;
        a1 = (int)v2;
      }
      while ( v7 != 0 );
      v5 = v2[212];
    }
  }
  else
  {
    for ( i = *(u32 *)(a1 + 844); ; i = v2[211] )
    {
      if ( i == 1 )
        v4 = http_opts_fn_24(v2, (int)v10, 0x80u, 0);
      else
        v4 = http_opts_fn_25((int)v2, (int)v10, 128, 0);
      if ( v4 < 0 )
      {
        v2[22] = 0;
        return 0;
      }
      if ( v4 == 0 )
        break;
      if ( v2[15] == 5 )
        goto LABEL_14;
    }
    v5 = v2[212];
  }
  if ( v5 == 1 )
  {
    http_io_fn_24(v1);
  }
  else
  {
    --v1[3];
    v1[4] = v2[11];
  }
  v2[22] = 0;
  return 0;
}


int http_io_fn_39(int a1)
{
  unsigned int v2;
  int v3;
  int v4;
  unsigned int v5;
  u8 *v6;
  int result;
  int v8;
  int v9;
  unsigned int v10;
  unsigned int v11;
  int (*i)(u8 *, u8 *, int);
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;

  v2 = 0;
  v3 = *(u32 *)(a1 + 124);
  if ( *(u16 *)(a1 + 234) == 1 )
  {
    v16 = sceNet_lib_D8722983("HTTP/");
    v17 = http_io_fn_5(a1, v3, v16, 0);
    if ( v17 == -2143088380 )
    {
      v4 = *(u32 *)(a1 + 176);
    }
    else
    {
      if ( v17 <= 0 )
        return -2143088540;
      if ( sceNet_lib_B9085A96(v3, "HTTP/", v17) != 0 )
      {
        sceNet_lib_80C9F02A(v3, "HTTP/0.9 200 OK\r\n\r\n");
        *(u32 *)(a1 + 68) = sceNet_lib_D8722983("HTTP/0.9 200 OK\r\n\r\n") + 1;
LABEL_22:
        v11 = *(u32 *)(a1 + 68);
        if ( v11 != 0 )
        {
          for ( i = http_core_fn_1; (unsigned int)i < v11; i = (int (*)(u8 *, u8 *, int))((char *)i + 1) )
            ;
        }
        return 0;
      }
      v4 = *(u32 *)(a1 + 176);
    }
  }
  else
  {
    v4 = *(u32 *)(a1 + 176);
  }
  v5 = 0;
  if ( v4 == 0 )
    goto LABEL_20;
  v6 = (u8 *)v3;
  while ( 1 )
  {
    result = http_opts_fn_25(a1, (int)v6, 1, 0);
    if ( result == 0 )
    {
      *v6 = 0;
      return -2143088540;
    }
    if ( result < 0 )
      return result;
    if ( v2 == 101 )
    {
      v15 = (char)*v6;
      v2 = 102;
      if ( v15 == 13 )
        goto LABEL_16;
      v14 = v15 ^ 0xA;
      v2 = 103;
      goto LABEL_34;
    }
    if ( v2 >= 0x66 )
    {
      v8 = (char)*v6;
      v2 = 103;
      goto LABEL_33;
    }
    if ( v2 == 100 )
    {
      v8 = (char)*v6;
      v2 = 101;
LABEL_33:
      v14 = v8 ^ 0xA;
LABEL_34:
      if ( v14 != 0 )
        v2 = 0;
      goto LABEL_16;
    }
    v9 = (char)*v6;
    if ( v9 == 0xA )
      v2 = 101;
    if ( v9 == 0xD )
      v2 = 100;
LABEL_16:
    if ( v2 == 103 )
      break;
    v10 = *(u32 *)(a1 + 176);
    if ( v5 == 1499 )
    {
      if ( v10 < 0x5DD )
      {
        v5 = 1500;
        goto LABEL_19;
      }
      v13 = http_opts_fn_2(v10);
      v3 = v13;
      if ( v13 == 0 )
        return -2143088606;
      sceKernelMemcpy(v13, *(u32 *)(a1 + 124), 1498);
      http_opts_fn_3(*(u32 *)(a1 + 124));
      v10 = *(u32 *)(a1 + 176);
      *(u32 *)(a1 + 124) = v3;
    }
    ++v5;
LABEL_19:
    v6 = (u8 *)(v3 + v5);
    if ( v5 >= v10 )
      goto LABEL_20;
  }
  *(u32 *)(a1 + 68) = v5 + 1;
LABEL_20:
  result = -2143088540;
  if ( v2 == 103 )
    goto LABEL_22;
  return result;
}


int http_io_fn_40(s64 a1)
{
  int v1;
  unsigned int v2;
  u64 v3;
  char *v4;
  int result;
  int v6;
  int v7;
  int v8; // dc

  v1 = a1;
  v2 = 0;
  v3 = 0;
  if ( (u32)a1 == 0 )
    return v3;
  while ( 1 )
  {
    v4 = (char *)(v1 + v2);
    result = v3;
    if ( *(u8 *)(v1 + v2) == 0 )
      break;
    v6 = sceNet_lib_750F705D(*(char *)(v1 + v2), HIDWORD(a1)) & 8;
    a1 = ((u64)(a1) & 0x00000000FFFFFFFFULL) | ((u64)(u32)((2 * ((v3 >> 30) + HIDWORD(v3) + (5 * (int)v3 < (unsigned int)v3))) | ((unsigned int)(5 * v3) >> 31)) << 32);
    v7 = v2++ < 0x15;
    if ( v6 != 0 )
      return v3;
    v8 = !v7;
    result = v3;
    if ( v8 )
      break;
    a1 = ((u64)(a1) & 0xFFFFFFFF00000000ULL) | (u32)(10 * v3);
    v3 = a1 + *v4 - 48;
  }
  return result;
}


int http_io_fn_41(u8 *a1, s64 a2)
{
  int v4;
  int v5;
  int v6;
  u8 *v7;
  int result;
  u8 *v9;

  v9 = a1;
  if ( a2 == 0 )
    return sceNet_lib_80C9F02A(a1, "0");
  v4 = http_core_fn_3(a2, HIDWORD(a2), 0xAu, 0);
  http_io_fn_46(&v9, v6, v4, v5);
  v7 = v9;
  result = http_core_fn_4(a2, 0xAu, 0);
  *v7 = result + 48;
  ++v9;
  return result;
}


int http_io_fn_42(int a1, int a2)
{
  int v2;
  int result;

  v2 = *(u32 *)(a1 + 40);
  if ( v2 != 0 )
  {
    *(u32 *)(a2 + 52) = v2;
    *(u32 *)(a1 + 40) = 0;
  }
  result = *(u32 *)(a1 + 44);
  if ( result != 0 )
  {
    *(u32 *)(a2 + 56) = result;
    *(u32 *)(a1 + 44) = 0;
  }
  return result;
}


int http_io_fn_43(int a1)
{
  int v1;
  int result;

  v1 = a1 & 0xF000000;
  result = 0x1000000;
  if ( (a1 & 0xF000000) != 0x1000000 )
  {
    result = 0x2000000;
    if ( (a1 & 0xF000000u) > 0x1000000 )
    {
      if ( v1 == 0x2000000 )
        return result;
    }
    else
    {
      result = 0;
      if ( v1 == 0 )
        return result;
    }
    return -2143088384;
  }
  return result;
}


int http_io_fn_44(int a1, int a2, int a3)
{
  int result;

  result = http_io_fn_107(a1, a2, 1);
  if ( result >= 0 )
    return http_io_fn_107(a1, a3, 2);
  return result;
}


int http_io_fn_45(int a1, int a2, int a3)
{
  int result;
  u8 v5[2];
  u16 v6[7];

  v5[0] = a2;
  v6[0] = a3;
  result = http_io_fn_108(a1, v5, 1);
  if ( result >= 0 )
    return http_io_fn_108(a1, v6, 2);
  return result;
}


void http_io_fn_46(u8 **a1, s64 a2)
{
  if ( a2 != 0 )
  {
    u64 q = ((u64 (*)(u64, u64))http_core_fn_3)((u64)a2, 10ULL);
    u64 r = ((u64 (*)(u64, u64))http_core_fn_4)((u64)a2, 10ULL);
    http_io_fn_46(a1, (s64)q);
    **a1 = (u8)(r + 48);
    ++*a1;
  }
}


int sceHttpInitCache(int a1)
{
  int v2; // dc
  int result;
  int v4;

  v2 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v2 )
  {
    v4 = 0;
    if ( HTTP_BSS_U32(0x315C) == 1 )
      return -2143088608;
    sceKernelMemset(&HTTP_BSS_U8(0x30DC), 0, 64);
    HTTP_BSS_U32(0x315C) = 1;
    HTTP_BSS_U32(0x3160) = a1;
    HTTP_BSS_U32(0x3164) = 0;
    HTTP_BSS_U32(0x3168) = 0;
    HTTP_BSS_U32(0x316C) = 0;
    return v4;
  }
  return result;
}


int sceHttpFlushCache()
{
  int v0; // dc
  int result;
  int v2;
  int v3;
  int v4;
  u32 *v5;
  int v6;
  u32 *v7;

  v0 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v0 )
  {
    if ( HTTP_BSS_U32(0x315C) == 1 )
    {
      v2 = http_io_fn_89();
      v5 = (u32 *)HTTP_BSS_U32(0x3168);
      v6 = v2;
      if ( HTTP_BSS_U32(0x3168) != 0 )
      {
        do
        {
          v7 = (u32 *)v5[15];
          http_opts_fn_21(v5);
          v5 = v7;
        }
        while ( v7 != NULL );
      }
      http_io_fn_90(v6, v3, v4);
      return 0;
    }
    else
    {
      return -2143088639;
    }
  }
  return result;
}


int sceHttpEndCache()
{
  int v0; // dc
  int result;
  int *v2;
  int *v3;

  v0 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v0 )
  {
    if ( HTTP_BSS_U32(0x315C) == 1 )
    {
      v2 = (int *)HTTP_BSS_U32(0x3168);
      if ( HTTP_BSS_U32(0x3168) != 0 )
      {
        do
        {
          v3 = (int *)v2[15];
          http_io_fn_55(v2[12]);
          http_opts_fn_3(v2[6]);
          http_opts_fn_3((int)v2);
          v2 = v3;
        }
        while ( v3 != NULL );
      }
      HTTP_BSS_U32(0x315C) = 0;
      HTTP_BSS_U32(0x3160) = 0;
      HTTP_BSS_U32(0x3164) = 0;
      HTTP_BSS_U32(0x3168) = 0;
      HTTP_BSS_U32(0x316C) = 0;
      sceKernelMemset(&HTTP_BSS_U8(0x30DC), 0, 64);
      return 0;
    }
    else
    {
      return -2143088639;
    }
  }
  return result;
}


int sceHttp_739C2D79(int a1, unsigned int a2)
{
  int v4; // dc
  int result;
  int v6;
  unsigned int v7;
  unsigned int i;
  u32 *v9;

  v4 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v4 )
  {
    v6 = 0;
    if ( HTTP_BSS_U32(0x3170) == 1 )
      return -2143088608;
    sceKernelMemset(&HTTP_BSS_U8(0x311C), 0, 64);
    HTTP_BSS_U32(0x3170) = 1;
    HTTP_BSS_U32(0x3174) = a1;
    HTTP_BSS_U32(0x3178) = a2;
    HTTP_BSS_U32(0x317C) = 0;
    HTTP_BSS_U32(0x3180) = 0;
    HTTP_BSS_U32(0x3184) = 0;
    HTTP_BSS_U32(0x3188) = 0;
    sceKernelMemset(a1, 0, a2);
    v7 = 0;
    if ( a2 >= 0x200 )
    {
      for ( i = 512; i <= a2; i += 512 )
      {
        v9 = (u32 *)(a1 + v7);
        v7 = i;
        *v9 = HTTP_BSS_U32(0x3180);
        HTTP_BSS_U32(0x3180) = (int)v9;
      }
    }
    return v6;
  }
  return result;
}


int sceHttp_B0257723()
{
  int v0; // dc
  int result;
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;

  v0 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v0 )
  {
    if ( HTTP_BSS_U32(0x3170) == 1 )
    {
      v2 = http_io_fn_89();
      v5 = HTTP_BSS_U32(0x3184);
      v6 = v2;
      if ( HTTP_BSS_U32(0x3184) != 0 )
      {
        do
        {
          v7 = *(u32 *)(v5 + 60);
          http_io_fn_62();
          v5 = v7;
        }
        while ( v7 != 0 );
      }
      http_io_fn_90(v6, v3, v4);
      return 0;
    }
    else
    {
      return -2143088639;
    }
  }
  return result;
}


int sceHttp_A461A167()
{
  int v0; // dc
  int result;

  v0 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v0 )
  {
    if ( HTTP_BSS_U32(0x3170) == 1 )
    {
      HTTP_BSS_U32(0x3170) = 0;
      HTTP_BSS_U32(0x3174) = 0;
      HTTP_BSS_U32(0x3178) = 0;
      HTTP_BSS_U32(0x317C) = 0;
      HTTP_BSS_U32(0x3180) = 0;
      HTTP_BSS_U32(0x3184) = 0;
      HTTP_BSS_U32(0x3188) = 0;
      sceKernelMemset(&HTTP_BSS_U8(0x311C), 0, 64);
      return 0;
    }
    else
    {
      return -2143088639;
    }
  }
  return result;
}


int http_io_fn_47(u32 *a1)
{
  int result;

  result = a1[1];
  ++*a1;
  a1[1] = result + 1;
  return result;
}


int http_io_fn_48(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  void *v6;
  int v7;
  int v8;

  v2 = 0;
  v3 = http_io_fn_14(a1);
  if ( v3 != 0 )
  {
    v4 = http_io_fn_89();
    v5 = a1[39];
    v6 = &HTTP_BSS_U8(0x30DC);
    v7 = v4;
    v8 = v3;
    if ( v5 == 1 || (v6 = &HTTP_BSS_U8(0x311C), v8 = v3, v5 == 2) )
      v2 = http_io_fn_72(v6, v8, a1[7], 0);
    http_io_fn_90(v7, v8, v5);
    http_opts_fn_3(v3);
  }
  return v2;
}


int http_io_fn_49(int a1, int a2, int a3, int a4)
{
  int v4;
  int v8;
  void *v10;
  int v11;
  int v12;
  int v13;
  int v14;

  v4 = 0;
  v8 = http_io_fn_89();
  v10 = &HTTP_BSS_U8(0x30DC);
  v11 = v8;
  v12 = a2;
  v13 = a3;
  v14 = a4;
  if ( a1 == 1 || (v12 = a2, v13 = a3, v14 = a4, v10 = &HTTP_BSS_U8(0x311C), a1 == 2) )
    v4 = http_io_fn_72(v10, v12, v13, v14);
  http_io_fn_90(v11, v12, v13);
  return v4;
}


int http_io_fn_50(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  int result;
  int v6;
  int v7;
  unsigned int v8;
  int v9;
  int v10;
  int v11;
  int v12;

  v2 = 0;
  v3 = a1[210];
  v10 = 0;
  v12 = 0;
  v9 = 0;
  v11 = 0;
  if ( v3 == 1 )
  {
    v7 = a1[209];
    v8 = a1[46];
    if ( v7 != 0 )
      return v2;
    result = 0;
    if ( v8 < a1[208] )
      return result;
    v4 = a1[31];
  }
  else
  {
    v4 = a1[31];
  }
  if ( sceParseHttpResponseHeader(v4, a1[17], "Pragma", &v9, &v10) > 0 )
    return v2;
  if ( sceParseHttpResponseHeader(a1[31], a1[17], "Cache-Control", &v11, &v12) > 0 )
    return 0;
  v6 = a1[39];
  if ( v6 == 1 )
  {
    result = 0;
    if ( a1[210] == 1 && *((u64 *)a1 + 104) != 0 )
      return http_io_fn_57(a1);
  }
  else
  {
    result = 0;
    if ( v6 == 2 )
      return http_io_fn_63(a1);
  }
  return result;
}


void http_io_fn_51(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  void *v5;
  u32 *v6;
  unsigned int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  unsigned int v12;
  int v13;
  int v14;
  unsigned int v15;

  if ( a1[235] != 0 )
  {
    v2 = http_io_fn_14(a1);
    if ( v2 == 0 )
    {
LABEL_11:
      v9 = a1[235];
      if ( v9 != 0 )
      {
        http_io_fn_52(v9);
        a1[235] = 0;
      }
      return;
    }
    v3 = http_io_fn_89();
    if ( HTTP_BSS_U32(0x3170) == 1 )
    {
      v15 = a1[235];
      if ( v15 >= HTTP_BSS_U32(0x3174) )
      {
        v4 = a1[7];
        if ( v15 < HTTP_BSS_U32(0x3174) + HTTP_BSS_U32(0x3178) )
        {
          v5 = &HTTP_BSS_U8(0x311C);
          goto LABEL_7;
        }
      }
      else
      {
        v4 = a1[7];
      }
    }
    else
    {
      v4 = a1[7];
    }
    v5 = &HTTP_BSS_U8(0x30DC);
LABEL_7:
    v6 = (u32 *)http_io_fn_72(v5, v2, v4, 0);
    if ( v6 != NULL )
    {
      if ( HTTP_BSS_U32(0x3170) == 1 && (unsigned int)v6 >= HTTP_BSS_U32(0x3174) && (unsigned int)v6 < HTTP_BSS_U32(0x3174) + HTTP_BSS_U32(0x3178) )
        http_io_fn_61(v6);
      else
        http_opts_fn_19(v6);
    }
    else if ( HTTP_BSS_U32(0x3170) == 1 && (v12 = a1[235]) >= HTTP_BSS_U32(0x3174) && v12 < HTTP_BSS_U32(0x3174) + HTTP_BSS_U32(0x3178) )
    {
      http_io_fn_70(&HTTP_BSS_U8(0x311C));
      *(u32 *)(a1[235] + 56) = 0;
      v7 = a1[235];
      *(u32 *)(v7 + 60) = HTTP_BSS_U32(0x3184);
      v13 = a1[235];
      v14 = *(u32 *)(v13 + 60);
      if ( v14 != 0 )
        *(u32 *)(v14 + 56) = v13;
      else
        HTTP_BSS_U32(0x3188) = a1[235];
      HTTP_BSS_U32(0x3184) = a1[235];
      a1[235] = 0;
    }
    else
    {
      http_io_fn_70(&HTTP_BSS_U8(0x30DC));
      v7 = 0x20000u;
      *(u32 *)(a1[235] + 56) = 0;
      *(u32 *)(a1[235] + 60) = HTTP_BSS_U32(0x3168);
      v10 = a1[235];
      v11 = *(u32 *)(v10 + 60);
      if ( v11 != 0 )
        *(u32 *)(v11 + 56) = v10;
      else
        HTTP_BSS_U32(0x316C) = a1[235];
      HTTP_BSS_U32(0x3168) = a1[235];
      a1[235] = 0;
    }
    http_io_fn_90(v3, v7, v8);
    http_opts_fn_3(v2);
    goto LABEL_11;
  }
}


int http_io_fn_52(u32 *a1)
{
  if ( HTTP_BSS_U32(0x3170) == 1 && (unsigned int)a1 >= HTTP_BSS_U32(0x3174) && (unsigned int)a1 < HTTP_BSS_U32(0x3174) + HTTP_BSS_U32(0x3178) )
  {
    http_io_fn_60(a1[12]);
    *a1 = 0;
    http_io_fn_59(a1);
    return 0;
  }
  else
  {
    http_io_fn_56(a1[12]);
    http_io_fn_55(a1[12]);
    http_opts_fn_3(a1[6]);
    return http_opts_fn_3((int)a1);
  }
}


unsigned int http_io_fn_53(int a1, u32 *a2, unsigned int a3, unsigned int a4)
{
  int v4;
  unsigned int v5;
  unsigned int v6;
  int v7;
  u32 *v8;
  unsigned int result;
  u32 *v10;
  int v11;
  unsigned int v12;
  int v13;
  unsigned int v14;

  v4 = a1;
  v5 = a3;
  v6 = a4;
  v7 = 0;
  if ( HTTP_BSS_U32(0x3170) != 1 )
  {
    v8 = (u32 *)*a2;
LABEL_3:
    sceKernelMemcpy(a1, (char *)v8 + a3, a4);
    return v6;
  }
  if ( (unsigned int)a2 < HTTP_BSS_U32(0x3174) )
  {
    v8 = (u32 *)*a2;
    goto LABEL_3;
  }
  if ( (unsigned int)a2 >= HTTP_BSS_U32(0x3174) + HTTP_BSS_U32(0x3178) )
  {
    v8 = (u32 *)*a2;
    goto LABEL_3;
  }
  v10 = (u32 *)*a2;
  v11 = *a2 != 0;
  if ( *a2 != 0 )
  {
    while ( 1 )
    {
      v12 = v10[1];
      v11 = v10 != NULL;
      if ( v5 < v12 )
        break;
      v10 = (u32 *)*v10;
      v5 -= v12;
      if ( v10 == NULL )
      {
        v11 = false;
        break;
      }
    }
  }
  result = 0;
  if ( v11 && a4 != 0 )
  {
    do
    {
      v13 = (int)v10 + v5 + 8;
      v14 = v10[1] - v5;
      if ( v14 >= v6 )
        v14 = v6;
      sceKernelMemcpy(v4, v13, v14);
      v10 = (u32 *)*v10;
      v6 -= v14;
      v7 += v14;
      v4 += v14;
      v5 = 0;
    }
    while ( v10 != NULL && v6 != 0 );
    return v7;
  }
  return result;
}


unsigned int http_io_fn_54(int *a1, int a2, int a3, unsigned int a4)
{
  int v4;
  int v5;
  unsigned int v6;
  int v7;
  int i;
  u32 *v10;
  int v11;
  int j;
  unsigned int v13;
  int v14;

  v4 = 0;
  v5 = a3;
  v6 = a4;
  if ( HTTP_BSS_U32(0x3170) != 1 )
  {
    v7 = *a1;
LABEL_3:
    sceKernelMemcpy(v7 + a2, a3, a4);
    return v6;
  }
  if ( (unsigned int)a1 < HTTP_BSS_U32(0x3174) )
  {
    v7 = *a1;
    goto LABEL_3;
  }
  if ( (unsigned int)a1 >= HTTP_BSS_U32(0x3174) + HTTP_BSS_U32(0x3178) )
  {
    v7 = *a1;
    goto LABEL_3;
  }
  i = *a1;
  if ( *a1 != 0 )
  {
    v10 = *(u32 **)i;
    v11 = i != 0;
    if ( *(u32 *)i == 0 )
      goto LABEL_15;
    for ( i = *(u32 *)i; ; i = (int)v10 )
    {
      v10 = (u32 *)*v10;
      if ( v10 == NULL )
        break;
    }
  }
  else
  {
    i = http_io_fn_58();
  }
  v11 = i != 0;
LABEL_15:
  if ( !v11 || v6 == 0 )
    return v4;
  for ( j = *(u32 *)(i + 4); ; j = *(u32 *)(i + 4) )
  {
    v13 = 504 - j;
    if ( 504 - j >= v6 )
      v13 = v6;
    sceKernelMemcpy(i + j + 8, v5, v13);
    v6 -= v13;
    v4 += v13;
    *(u32 *)(i + 4) += v13;
    v5 += v13;
    if ( v6 != 0 )
    {
      v14 = http_io_fn_58();
      *(u32 *)i = v14;
      v11 = v14 != 0;
      i = v14;
    }
    if ( !v11 || v6 == 0 )
      break;
  }
  return v4;
}


int http_io_fn_55(u32 *a1)
{
  int v2;
  int v3;
  int v4;

  v2 = a1[6];
  if ( v2 != 0 )
    http_opts_fn_3(v2);
  v3 = a1[7];
  if ( v3 != 0 )
    http_opts_fn_3(v3);
  v4 = a1[8];
  if ( v4 != 0 )
    http_opts_fn_3(v4);
  return http_opts_fn_3((int)a1);
}


void http_io_fn_56(int a1)
{
  HTTP_BSS_U32(0x3164) -= *(u32 *)(a1 + 16) + *(u32 *)(a1 + 12);
}


u32 *http_io_fn_57(u32 *a1)
{
  int v2;
  u32 *v3;
  int v4;
  int v5;
  int v6;
  int v7;
  u32 *v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v17;

  v2 = 0;
  v3 = NULL;
  v4 = http_io_fn_89();
  v7 = http_io_fn_73(a1[17] + a1[208]);
  http_io_fn_90(v4, v5, v6);
  v8 = NULL;
  if ( v7 != 1 )
    goto LABEL_11;
  v2 = http_io_fn_14(a1);
  if ( v2 == 0 )
    goto LABEL_11;
  v3 = (u32 *)http_opts_fn_2(68);
  if ( v3 == NULL )
    goto LABEL_11;
  v9 = http_opts_fn_2(40);
  v8 = (u32 *)v9;
  if ( v9 == 0 )
    goto LABEL_11;
  sceKernelMemset(v9, 0, 40);
  v10 = a1[17];
  if ( v10 != 0 )
  {
    v8[4] = v10;
    v17 = http_opts_fn_2(a1[17]);
    v8[7] = v17;
    if ( v17 == 0 )
      goto LABEL_11;
  }
  v11 = a1[209];
  v12 = a1[208];
  if ( *((u64 *)a1 + 104) == 0 || (v8[3] = a1[208], v11 == 0) && (v13 = http_opts_fn_2(v12), v8[8] = v13, v13 != 0) )
  {
    sceKernelMemset(v3, 0, 68);
    v14 = sceNet_lib_D8722983(v2);
    http_io_fn_91(v2, v14, v3 + 2);
    v15 = a1[7];
    v3[6] = v2;
    v3[7] = v15;
    v3[12] = v8;
    return v3;
  }
  else
  {
LABEL_11:
    if ( v7 == 1 )
      HTTP_BSS_U32(0x3164) -= a1[17] + a1[208];
    if ( v2 != 0 )
      http_opts_fn_3(v2);
    if ( v3 != NULL )
      http_opts_fn_3((int)v3);
    if ( v8 != NULL )
      http_io_fn_55(v8);
    return NULL;
  }
}


u32 *http_io_fn_58()
{
  int v0;
  int v1;
  int v2;
  u32 *v3;
  int v4;
  int i;

  v2 = http_io_fn_89();
  while ( 1 )
  {
    v3 = (u32 *)HTTP_BSS_U32(0x3180);
    if ( HTTP_BSS_U32(0x3180) != 0 )
      break;
    v4 = HTTP_BSS_U32(0x3188);
    if ( HTTP_BSS_U32(0x3188) == 0 )
      goto LABEL_9;
    for ( i = *(u32 *)(HTTP_BSS_U32(0x3188) + 4); i != 0; i = *(u32 *)(v4 + 4) )
    {
      v4 = *(u32 *)(v4 + 56);
      if ( v4 == 0 )
        goto LABEL_9;
    }
    http_io_fn_62();
  }
  HTTP_BSS_U32(0x3180) = *(u32 *)HTTP_BSS_U32(0x3180);
  *v3 = 0;
LABEL_9:
  http_io_fn_90(v2, v0, v1);
  if ( v3 != NULL )
  {
    sceKernelMemset(v3, 0, 512);
    ++HTTP_BSS_U32(0x317C);
  }
  return v3;
}


void http_io_fn_59(u32 **a1)
{
  u32 *v2;
  u32 *v3;
  int v4; // dc
  int v5;
  int v6;
  int v7;

  if ( a1 != NULL )
  {
    v2 = *a1;
    v3 = a1;
    v4 = *a1 == NULL;
    --HTTP_BSS_U32(0x317C);
    if ( !v4 )
    {
      do
      {
        v3 = v2;
        v2 = (u32 *)*v2;
        --HTTP_BSS_U32(0x317C);
      }
      while ( v2 != NULL );
    }
    v5 = http_io_fn_89();
    *v3 = HTTP_BSS_U32(0x3180);
    HTTP_BSS_U32(0x3180) = (int)a1;
    http_io_fn_90(v5, v6, v7);
  }
}


void http_io_fn_60(u32 *a1)
{
  u32 **v2;
  u32 **v3;
  u32 **v4;

  v2 = (u32 **)a1[6];
  if ( v2 != NULL )
    http_io_fn_59(v2);
  v3 = (u32 **)a1[7];
  if ( v3 != NULL )
    http_io_fn_59(v3);
  v4 = (u32 **)a1[8];
  if ( v4 != NULL )
    http_io_fn_59(v4);
}


void http_io_fn_61(int a1)
{
  int v2;
  int v3;
  int v4;
  int v5;

  v2 = *(u32 *)(a1 + 4);
  *(u32 *)(a1 + 4) = v2 - 1;
  if ( v2 == 1 && *(u32 *)(a1 + 52) == 1 )
  {
    http_io_fn_71(&HTTP_BSS_U8(0x311C), a1);
    v3 = *(u32 *)(a1 + 56);
    if ( v3 != 0 )
    {
      *(u32 *)(v3 + 60) = *(u32 *)(a1 + 60);
      v5 = *(u32 *)(a1 + 60);
      if ( v5 != 0 )
        *(u32 *)(v5 + 56) = v3;
      else
        HTTP_BSS_U32(0x3188) = v3;
    }
    else
    {
      HTTP_BSS_U32(0x3184) = *(u32 *)(a1 + 60);
      v4 = *(u32 *)(a1 + 60);
      if ( v4 != 0 )
        *(u32 *)(v4 + 56) = 0;
      else
        HTTP_BSS_U32(0x3188) = 0;
    }
    http_io_fn_60(*(u32 **)(a1 + 48));
    *(u32 *)a1 = 0;
    http_io_fn_59((u32 **)a1);
  }
}


void http_io_fn_62(int a1)
{
  int v2;
  int v3;
  int v4;
  int v5;

  v2 = *(u32 *)(a1 + 4);
  *(u32 *)(a1 + 52) = 1;
  if ( v2 == 0 )
  {
    http_io_fn_71(&HTTP_BSS_U8(0x311C), a1);
    v3 = *(u32 *)(a1 + 56);
    if ( v3 != 0 )
    {
      *(u32 *)(v3 + 60) = *(u32 *)(a1 + 60);
      v5 = *(u32 *)(a1 + 60);
      if ( v5 != 0 )
        *(u32 *)(v5 + 56) = v3;
      else
        HTTP_BSS_U32(0x3188) = v3;
    }
    else
    {
      HTTP_BSS_U32(0x3184) = *(u32 *)(a1 + 60);
      v4 = *(u32 *)(a1 + 60);
      if ( v4 != 0 )
        *(u32 *)(v4 + 56) = 0;
      else
        HTTP_BSS_U32(0x3188) = 0;
    }
    http_io_fn_60(*(u32 **)(a1 + 48));
    *(u32 *)a1 = 0;
    http_io_fn_59((u32 **)a1);
  }
}


u32 *http_io_fn_63(int a1)
{
  int v2;
  int v3;
  u32 *v4;
  u32 *v5;
  int v6;
  u32 *v7;
  u32 *v8;
  int v9;
  int v10;
  u32 *v12;

  v2 = http_io_fn_14((u32 *)a1);
  v3 = v2;
  if ( v2 != 0 )
  {
    if ( (unsigned int)(sceNet_lib_D8722983(v2) + 1) < 0x191 )
    {
      v4 = http_io_fn_58();
      v5 = v4;
      if ( v4 != NULL )
      {
        v6 = *(u32 *)(a1 + 68);
        v4[6] = v4 + 28;
        v7 = v4 + 18;
        if ( v6 == 0 || (v4[22] = v6, v12 = http_io_fn_58(), v7[7] = v12, v12 != NULL) )
        {
          if ( *(u32 *)(a1 + 840) == 1 && *(u64 *)(a1 + 832) == 0
            || (v7[3] = *(u32 *)(a1 + 832), v8 = http_io_fn_58(), v7[8] = v8, v8 != NULL) )
          {
            v9 = sceNet_lib_D8722983(v3);
            http_io_fn_91(v3, v9, v5 + 2);
            sceNet_lib_80C9F02A(v5[6], v3);
            v10 = *(u32 *)(a1 + 28);
            v5[12] = v7;
            v5[7] = v10;
            http_opts_fn_3(v3);
            return v5;
          }
        }
        if ( v7 != NULL )
          http_io_fn_60(v7);
        if ( v5 != NULL )
        {
          *v5 = 0;
          http_io_fn_59((u32 **)v5);
        }
      }
    }
    if ( v3 == 0 )
      return NULL;
    http_opts_fn_3(v3);
  }
  return NULL;
}


int http_io_fn_64(u32 *a1)
{
  int v2;
  int v3;
  int v4;

  v2 = http_io_fn_89();
  if ( HTTP_BSS_U32(0x3170) == 1 && (unsigned int)a1 >= HTTP_BSS_U32(0x3174) && (unsigned int)a1 < HTTP_BSS_U32(0x3174) + HTTP_BSS_U32(0x3178) )
    http_io_fn_61((int)a1);
  else
    http_opts_fn_19(a1);
  return http_io_fn_90(v2, v3, v4);
}


int http_io_fn_65(u32 *a1)
{
  int v2;
  int v3;
  int v4;

  v2 = http_io_fn_89();
  if ( HTTP_BSS_U32(0x3170) == 1 && (unsigned int)a1 >= HTTP_BSS_U32(0x3174) && (unsigned int)a1 < HTTP_BSS_U32(0x3174) + HTTP_BSS_U32(0x3178) )
  {
    http_io_fn_62((int)a1);
    http_io_fn_61((int)a1);
  }
  else
  {
    http_opts_fn_21(a1);
    http_opts_fn_19(a1);
  }
  return http_io_fn_90(v2, v3, v4);
}


int http_io_fn_66()
{
  return HTTP_BSS_U32(0x315C);
}


int http_io_fn_67()
{
  return HTTP_BSS_U32(0x3160);
}


int http_io_fn_68()
{
  return HTTP_BSS_U32(0x3170);
}


int http_io_fn_69()
{
  return HTTP_BSS_U32(0x3178);
}


int http_io_fn_70(int a1, int a2)
{
  int v4;
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 *v10;

  v4 = 4 * (*(u8 *)(a2 + 8) >> 4);
  result = v4 + a1;
  v6 = *(u32 *)(v4 + a1);
  v7 = 0;
  if ( v6 != 0 )
  {
    v8 = a2 + 8;
    v9 = v6 + 8;
    do
    {
      result = sceNet_lib_E0A81C7C(v9, v8, 16);
      if ( result >= 0 )
        break;
      v7 = v6;
      v6 = *(u32 *)(v6 + 64);
      v9 = v6 + 8;
    }
    while ( v6 != 0 );
    v10 = (u32 *)(v4 + a1);
    if ( v7 != 0 )
    {
      *(u32 *)(a2 + 64) = *(u32 *)(v7 + 64);
      *(u32 *)(v7 + 64) = a2;
      return result;
    }
  }
  else
  {
    v10 = (u32 *)(v4 + a1);
  }
  *(u32 *)(a2 + 64) = *v10;
  *v10 = a2;
  return result;
}


int http_io_fn_71(int a1, int a2)
{
  int result;
  int v3;
  int v4;

  result = 4 * (*(u8 *)(a2 + 8) >> 4);
  v3 = *(u32 *)(result + a1);
  v4 = 0;
  if ( v3 != 0 )
  {
    while ( v3 != a2 )
    {
      v4 = v3;
      v3 = *(u32 *)(v3 + 64);
      if ( v3 == 0 )
        return result;
    }
    if ( v4 != 0 )
      *(u32 *)(v4 + 64) = *(u32 *)(v3 + 64);
    else
      *(u32 *)(result + a1) = *(u32 *)(v3 + 64);
    *(u32 *)(v3 + 64) = 0;
  }
  return result;
}


u32 *http_io_fn_72(int a1, int a2, int a3, int a4)
{
  int v8;
  u32 *v9;
  int v10;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  u8 v19[16];

  v8 = sceNet_lib_D8722983(a2);
  http_io_fn_91(a2, v8, v19);
  v9 = *(u32 **)(4 * (v19[0] >> 4) + a1);
  if ( v9 == NULL )
    return v9;
  while ( 1 )
  {
    v10 = sceNet_lib_E0A81C7C(v9 + 2, v19, 16);
    if ( v10 == 0 )
      break;
    if ( v10 >= 0 )
      return NULL;
    v9 = (u32 *)v9[16];
LABEL_6:
    if ( v9 == NULL )
      return v9;
  }
  if ( sceNet_lib_A0F16ABD(v9[6], a2) != 0 )
  {
    v9 = (u32 *)v9[16];
    goto LABEL_6;
  }
  if ( v9[7] != a3 )
  {
    v9 = (u32 *)v9[16];
    goto LABEL_6;
  }
  if ( a4 != 0 && sceNet_lib_E0A81C7C(v9 + 8, a4, 16) != 0 )
  {
    v9 = (u32 *)v9[16];
    goto LABEL_6;
  }
  if ( v9[13] == 1 )
  {
    v9 = (u32 *)v9[16];
    goto LABEL_6;
  }
  http_io_fn_47(v9);
  if ( HTTP_BSS_U32(0x3170) != 1 )
  {
    v12 = v9[14];
    goto LABEL_19;
  }
  if ( (unsigned int)v9 < HTTP_BSS_U32(0x3174) )
  {
    v12 = v9[14];
    goto LABEL_19;
  }
  v12 = v9[14];
  if ( (unsigned int)v9 >= HTTP_BSS_U32(0x3174) + HTTP_BSS_U32(0x3178) )
  {
LABEL_19:
    if ( v12 != 0 )
    {
      *(u32 *)(v12 + 60) = v9[15];
      v15 = v9[15];
      if ( v15 == 0 )
      {
        HTTP_BSS_U32(0x316C) = v12;
        v9[14] = 0;
        goto LABEL_24;
      }
      *(u32 *)(v15 + 56) = v12;
    }
    else
    {
      HTTP_BSS_U32(0x3168) = v9[15];
      v13 = v9[15];
      if ( v13 != 0 )
        *(u32 *)(v13 + 56) = 0;
      else
        HTTP_BSS_U32(0x316C) = 0;
    }
    v9[14] = 0;
LABEL_24:
    v14 = HTTP_BSS_U32(0x3168);
    v9[15] = HTTP_BSS_U32(0x3168);
    if ( v14 != 0 )
      *(u32 *)(v14 + 56) = v9;
    else
      HTTP_BSS_U32(0x316C) = (int)v9;
    HTTP_BSS_U32(0x3168) = (int)v9;
    return v9;
  }
  if ( v12 != 0 )
  {
    *(u32 *)(v12 + 60) = v9[15];
    v18 = v9[15];
    if ( v18 != 0 )
    {
      *(u32 *)(v18 + 56) = v12;
      goto LABEL_38;
    }
    HTTP_BSS_U32(0x3188) = v12;
    v9[14] = 0;
  }
  else
  {
    HTTP_BSS_U32(0x3184) = v9[15];
    v16 = v9[15];
    if ( v16 != 0 )
      *(u32 *)(v16 + 56) = 0;
    else
      HTTP_BSS_U32(0x3188) = 0;
LABEL_38:
    v9[14] = 0;
  }
  v17 = HTTP_BSS_U32(0x3184);
  v9[15] = HTTP_BSS_U32(0x3184);
  if ( v17 != 0 )
    *(u32 *)(v17 + 56) = v9;
  else
    HTTP_BSS_U32(0x3188) = (int)v9;
  HTTP_BSS_U32(0x3184) = (int)v9;
  return v9;
}


int http_io_fn_73(int a1)
{
  u32 *v2;
  int result;
  int i;

  while ( HTTP_BSS_U32(0x3160) < (unsigned int)(HTTP_BSS_U32(0x3164) + a1) )
  {
    v2 = (u32 *)HTTP_BSS_U32(0x316C);
    result = 0;
    if ( HTTP_BSS_U32(0x316C) == 0 )
      return result;
    for ( i = *(u32 *)(HTTP_BSS_U32(0x316C) + 4); i != 0; i = v2[1] )
    {
      v2 = (u32 *)v2[14];
      if ( v2 == NULL )
        return 0;
    }
    http_opts_fn_21(v2);
  }
  HTTP_BSS_U32(0x3164) += a1;
  return 1;
}


int sceHttpGetContentLength(int a1, u32 *a2)
{
  int v4; // dc
  int result;
  int v6;
  u32 *v7;
  int v8;
  int v9;
  u32 *v10;
  int v11;
  int v12;
  int v13;
  int v14;
  u32 *v15;

  v4 = _sceKernelCheckThreadStack() < 10208;
  result = -2143088522;
  if ( v4 )
    return result;
  v6 = http_opts_fn_23();
  v7 = v15;
  if ( v6 == 1 )
  {
    v8 = http_io_fn_12(a1, &v15);
    if ( v8 >= 0 )
    {
      v7 = v15;
      v12 = v15[15];
      v8 = -2143088539;
      if ( v12 == 0 )
        goto LABEL_4;
      v8 = -2143088541;
      if ( v12 == 7 )
        goto LABEL_4;
      v13 = v15[233];
      if ( v13 != 0 )
        v12 = v15[15];
      else
        v13 = v15[232];
      v8 = -2143088532;
      if ( v12 == 8 )
        goto LABEL_4;
      if ( v12 != 3 )
      {
LABEL_16:
        if ( v7[210] != 0 )
        {
          v14 = v7[209];
          *a2 = v7[208];
          a2[1] = v14;
          v8 = 0;
        }
        else
        {
          v8 = -2143088526;
          if ( v7[211] != 1 )
            v8 = -2143088527;
        }
        goto LABEL_4;
      }
      v8 = -2143088530;
      if ( *(u32 *)(v13 + 16) != v15[6] )
        goto LABEL_4;
      v8 = http_io_fn_84(v15);
      if ( v8 >= 0 )
      {
        v7 = v15;
        goto LABEL_16;
      }
    }
    v7 = v15;
    goto LABEL_4;
  }
  v8 = -2143088639;
LABEL_4:
  if ( v7 != NULL )
  {
    v9 = http_io_fn_89();
    v10 = v15;
    --v15[8];
    http_io_fn_90(v9, v10, v11);
  }
  return v8;
}


int sceHttpGetStatusCode(int a1, u32 *a2)
{
  int v4; // dc
  int result;
  int v6;
  u32 *v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  u32 *v14;

  v4 = _sceKernelCheckThreadStack() < 10208;
  result = -2143088522;
  if ( v4 )
    return result;
  v6 = http_opts_fn_23();
  v7 = v14;
  if ( v6 == 1 )
  {
    v8 = http_io_fn_12(a1, &v14);
    if ( v8 >= 0 )
    {
      v7 = v14;
      v12 = v14[233];
      if ( v12 == 0 )
        v12 = v14[232];
      v13 = v14[15];
      v8 = -2143088539;
      if ( v13 == 0 )
        goto LABEL_4;
      v8 = -2143088541;
      if ( v13 == 7 )
        goto LABEL_4;
      v8 = -2143088532;
      if ( v13 == 8 )
        goto LABEL_4;
      if ( v13 != 3 )
        goto LABEL_15;
      v8 = -2143088530;
      if ( *(u32 *)(v12 + 16) != v14[6] )
        goto LABEL_4;
      v8 = http_io_fn_84(v14);
      if ( v8 >= 0 )
      {
        v7 = v14;
LABEL_15:
        v8 = 0;
        *a2 = v7[215];
        goto LABEL_4;
      }
    }
    v7 = v14;
    goto LABEL_4;
  }
  v8 = -2143088639;
LABEL_4:
  if ( v7 != NULL )
  {
    v9 = http_io_fn_89();
    --v14[8];
    http_io_fn_90(v9, v10, v11);
  }
  return v8;
}


int sceHttpGetNetworkErrno(int a1, int *a2)
{
  int v4; // dc
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13[4];

  v4 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v4 )
  {
    v13[0] = 0;
    v7 = -2143088639;
    v6 = http_opts_fn_23();
    v8 = a1;
    if ( v6 == 1 && (v7 = http_io_fn_12(a1, v13)) >= 0 )
    {
      v9 = v13[0];
      v7 = 0;
      v8 = *(u32 *)(v13[0] + 36);
      *a2 = v8;
    }
    else
    {
      v9 = v13[0];
    }
    if ( v9 != 0 )
    {
      v10 = http_io_fn_89(v8);
      v11 = v13[0];
      --*(u32 *)(v13[0] + 32);
      http_io_fn_90(v10, v11, v12);
    }
    return v7;
  }
  return result;
}


int sceHttpGetNetworkPspError(int a1, int *a2)
{
  int v4; // dc
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13[4];

  v4 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v4 )
  {
    v13[0] = 0;
    v7 = -2143088639;
    v6 = http_opts_fn_23();
    v8 = a1;
    if ( v6 == 1 && (v7 = http_io_fn_12(a1, v13)) >= 0 )
    {
      v9 = v13[0];
      v7 = 0;
      v8 = *(u32 *)(v13[0] + 40);
      *a2 = v8;
    }
    else
    {
      v9 = v13[0];
    }
    if ( v9 != 0 )
    {
      v10 = http_io_fn_89(v8);
      v11 = v13[0];
      --*(u32 *)(v13[0] + 32);
      http_io_fn_90(v10, v11, v12);
    }
    return v7;
  }
  return result;
}


int sceHttpGetAllHeader(int a1, u32 *a2, u32 *a3)
{
  int v6; // dc
  int result;
  int v8;
  u32 *v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  u32 *v17;

  v6 = _sceKernelCheckThreadStack() < 10208;
  result = -2143088522;
  if ( v6 )
    return result;
  v17 = NULL;
  v8 = http_opts_fn_23();
  v9 = NULL;
  if ( v8 == 1 )
  {
    v10 = http_io_fn_12(a1, &v17);
    if ( v10 >= 0 )
    {
      v9 = v17;
      v14 = v17[233];
      if ( v14 == 0 )
        v14 = v17[232];
      v15 = v17[15];
      switch ( v15 )
      {
        case 0:
          v10 = -2143088539;
          goto LABEL_4;
        case 7:
          v10 = -2143088541;
          goto LABEL_4;
        case 8:
          v10 = -2143088532;
          goto LABEL_4;
        case 3:
          v10 = -2143088530;
          if ( *(u32 *)(v14 + 16) != v17[6] )
            goto LABEL_4;
          v10 = http_io_fn_84(v17);
          if ( v10 < 0 )
            goto LABEL_19;
          v9 = v17;
          break;
        default:
          break;
      }
      if ( v9[32] != 0 )
      {
LABEL_16:
        v16 = v9[23];
        *a2 = v9[32];
        *a3 = v16;
        goto LABEL_4;
      }
      v10 = http_io_fn_88();
      if ( v10 >= 0 )
      {
        v9 = v17;
        goto LABEL_16;
      }
    }
LABEL_19:
    v9 = v17;
    goto LABEL_4;
  }
  v10 = -2143088639;
LABEL_4:
  if ( v9 != NULL )
  {
    v11 = http_io_fn_89(v9);
    --v17[8];
    http_io_fn_90(v11, v12, v13);
  }
  return v10;
}


int http_io_fn_74(u32 *a1)
{
  int v2;
  unsigned int v3;
  int v4;
  int result;
  int v6;

  v2 = a1[234];
  v3 = *(u32 *)(*(u32 *)(v2 + 48) + 16);
  if ( v3 >= 0x5DC )
  {
    v4 = http_opts_fn_2(v3 + 1);
    result = -2143088606;
    if ( v4 == 0 )
      return result;
    http_opts_fn_3(a1[31]);
    a1[31] = v4;
    v2 = a1[234];
  }
  http_io_fn_53(a1[31], (u32 *)(*(u32 *)(v2 + 48) + 28), 0, *(u32 *)(*(u32 *)(v2 + 48) + 16));
  *(u8 *)(a1[31] + *(u32 *)(*(u32 *)(a1[234] + 48) + 16)) = 0;
  v6 = a1[31];
  a1[17] = *(u32 *)(*(u32 *)(a1[234] + 48) + 16);
  http_io_fn_82(v6);
  a1[210] = 1;
  a1[208] = *(u32 *)(*(u32 *)(a1[234] + 48) + 12);
  a1[209] = 0;
  return 0;
}


int http_io_fn_75(int a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  u8 *v6;
  int result;
  int v8;
  int v9;
  unsigned int v10[6];

  v2 = 0;
  v3 = 0;
  v4 = *(u32 *)(a1 + 932);
  v9 = 0;
  v10[0] = 0;
  if ( v4 == 0 )
    v4 = *(u32 *)(a1 + 928);
  v5 = http_cookie_fn_18(&v8, *(u32 *)(v4 + 56), *(u32 *)(a1 + 112), *(u32 *)(v4 + 4), &v9);
  if ( v5 >= 0 )
  {
    v5 = http_cookie_fn_20(NULL, v8, (int *)v10, 0);
    if ( v5 >= 0 )
    {
      v5 = -2143088606;
      v6 = (u8 *)http_opts_fn_2(v10[0]);
      v2 = (int)v6;
      if ( v6 != NULL )
      {
        v5 = http_cookie_fn_20(v6, v8, NULL, v10[0]);
        if ( v5 >= 0 )
        {
          if ( *(u32 *)(a1 + 264) != 0 )
          {
            v3 = http_io_fn_14((u32 *)a1);
            _sceKernelCheckThreadStack();
            v5 = (*(int (**)(u32, int, int, u32))(a1 + 264))(
                   *(u32 *)(a1 + 24),
                   v3,
                   v2,
                   *(u32 *)(a1 + 268));
          }
          if ( v5 >= 0 )
            http_opts_fn_7(*(u32 *)(a1 + 24), (int)"Cookie", v2, 0);
        }
      }
    }
  }
  if ( v8 != 0 || v9 != 0 )
    http_cookie_fn_19(v8, v9);
  if ( v3 != 0 )
    http_opts_fn_3(v3);
  result = v5;
  if ( v2 != 0 )
  {
    http_opts_fn_3(v2);
    return v5;
  }
  return result;
}


int http_io_fn_76(int a1, int a2, int a3, int a4)
{
  int v8;
  u8 *v9;
  int v10;
  char *v11;
  unsigned int v13[4];

  v13[0] = 0;
  v8 = http_core_fn_11(NULL, v13, 0, a2, a3);
  if ( v8 >= 0 )
  {
    v8 = -2143088606;
    v9 = (u8 *)http_opts_fn_2(v13[0]);
    if ( v9 != NULL )
    {
      v8 = http_core_fn_11(v9, NULL, v13[0], a2, a3);
      if ( v8 >= 0 )
      {
        v10 = (int)v9;
        if ( a4 == 1 )
        {
          v11 = "Proxy-Authorization";
        }
        else
        {
          v11 = "Authorization";
          v10 = (int)v9;
        }
        v8 = http_opts_fn_7(*(u32 *)(a1 + 24), (int)v11, v10, 0);
      }
    }
    if ( v9 != NULL )
      http_opts_fn_3((int)v9);
  }
  return v8;
}


int http_io_fn_77(int *a1, int a2, int a3, int a4, int a5, u32 *a6, int a7)
{
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  char *v19;
  int v21[4];

  v14 = a1[7];
  v15 = a1[28];
  v21[0] = 0;
  v16 = http_core_fn_12(0, v21, 0, a2, a3, v15, v14, a4, a5, a6);
  if ( v16 >= 0 )
  {
    v16 = -2143088606;
    v17 = http_opts_fn_2(v21[0]);
    if ( v17 != 0 )
    {
      v16 = http_core_fn_12(v17, NULL, v21[0], a2, a3, a1[28], a1[7], a4, a5, a6);
      if ( v16 >= 0 )
      {
        v18 = v17;
        if ( a7 == 1 )
        {
          v19 = "Proxy-Authorization";
        }
        else
        {
          v19 = "Authorization";
          v18 = v17;
        }
        v16 = http_opts_fn_7(a1[6], (int)v19, v18, 0);
      }
    }
    if ( v17 != 0 )
      http_opts_fn_3(v17);
  }
  return v16;
}


int http_io_fn_78(int **a1, int a2, int a3)
{
  int v6;
  int *v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int result;
  int v14;

  if ( a2 == 0 || a3 == 0 || a1 == NULL )
    return -2143088130;
  *a1 = NULL;
  v6 = http_opts_fn_2(12);
  v7 = (int *)v6;
  v8 = -2143088606;
  if ( v6 != 0 )
  {
    sceKernelMemset(v6, 0, 12);
    v10 = sceNet_lib_D8722983(a2);
    v9 = http_opts_fn_2(v10 + 1);
    *v7 = v9;
    v8 = -2143088606;
    if ( v9 != 0 )
    {
      sceKernelMemcpy(v9, a2, v10);
      *(u8 *)(*v7 + v10) = 0;
      if ( a3 == 0 )
      {
LABEL_8:
        result = 0;
        *a1 = v7;
        return result;
      }
      v12 = sceNet_lib_D8722983(a3);
      v11 = http_opts_fn_2(v12 + 1);
      v7[1] = v11;
      v8 = -2143088606;
      if ( v11 != 0 )
      {
        sceKernelMemcpy(v11, a3, v12);
        *(u8 *)(v7[1] + v12) = 0;
        goto LABEL_8;
      }
    }
  }
  result = -2143088606;
  if ( v7 != NULL )
  {
    if ( *v7 != 0 )
      http_opts_fn_3(*v7);
    v14 = v7[1];
    if ( v14 != 0 )
      http_opts_fn_3(v14);
    http_opts_fn_3((int)v7);
    return v8;
  }
  return result;
}


int http_io_fn_79(int *a1)
{
  int *v1;
  int i;
  int v3;
  int *v4;
  int v5;
  int result;

  v1 = a1;
  if ( a1 != NULL )
  {
    for ( i = *a1; ; i = *v4 )
    {
      if ( i != 0 )
        http_opts_fn_3(i);
      v3 = v1[1];
      if ( v3 != 0 )
        http_opts_fn_3(v3);
      v4 = (int *)v1[2];
      v5 = (int)v1;
      v1 = v4;
      result = http_opts_fn_3(v5);
      if ( v4 == NULL )
        break;
    }
  }
  return result;
}


int http_io_fn_80(int a1, int a2, int a3)
{
  int v6;
  int v7;
  char *v8;
  int v9;
  int v10;
  s16 v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  char *v19;
  int v20;
  u32 *v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  char *v27;
  int v28;
  int v29;
  int v31;
  int v32;
  int v33;
  u32 v34[5];
  char *v35;
  int v36;
  int v37;
  s16 v38;
  u8 v39[32];
  u32 v40[4];

  v40[0] = 0;
  sceKernelMemset(v34, 0, 44);
  v6 = *(u32 *)(a1 + 932);
  if ( v6 == 0 )
    v6 = *(u32 *)(a1 + 928);
  v7 = 0;
  switch ( *(u32 *)(a1 + 28) )
  {
    case 0:
      v8 = "GET";
      goto LABEL_5;
    case 1:
      v8 = "POST";
      goto LABEL_5;
    case 2:
      v8 = "HEAD";
      goto LABEL_5;
    case 3:
      v8 = "OPTIONS";
      goto LABEL_5;
    case 4:
      v8 = "PUT";
      goto LABEL_5;
    case 5:
      v8 = "DELETE";
      goto LABEL_5;
    case 6:
      v8 = "TRACE";
      goto LABEL_5;
    case 7:
      v8 = "CONNECT";
LABEL_5:
      v9 = sceNet_lib_D8722983(v8);
      if ( a3 == 0 )
        sceNet_lib_5705F6F9(a2, v8);
      v10 = v9 + 1;
      if ( a3 == 0 )
        sceNet_lib_5705F6F9(a2, " ");
      v11 = *(u16 *)(v6 + 400);
      v12 = *(u32 *)(a1 + 28);
      v34[4] = *(u32 *)(v6 + 56);
      v38 = v11;
      if ( v12 != 7 )
      {
        v13 = *(u32 *)(a1 + 116);
        v14 = *(u32 *)(a1 + 120);
        v35 = *(char **)(a1 + 112);
        v36 = v13;
        v37 = v14;
      }
      if ( v35 == NULL )
        goto LABEL_12;
      if ( sceNet_lib_D8722983(v35) == 0 )
      {
        v12 = *(u32 *)(a1 + 28);
LABEL_12:
        v35 = "/";
        goto LABEL_13;
      }
      if ( sceNet_lib_D8722983(v35) == 1 )
      {
        v12 = *(u32 *)(a1 + 28);
        if ( *v35 == 32 )
          goto LABEL_12;
      }
      else
      {
        v12 = *(u32 *)(a1 + 28);
      }
LABEL_13:
      if ( v12 == 7 )
      {
        v15 = 6;
        v34[0] = 1;
      }
      else
      {
        if ( *(u32 *)(v6 + 192) != 1 || *(u32 *)(v6 + 240) == 0 )
        {
          v34[0] = 1;
LABEL_16:
          v15 = 200;
          goto LABEL_17;
        }
        if ( *(u32 *)(v6 + 4) != 0 )
        {
          v34[0] = 1;
          goto LABEL_16;
        }
        v15 = 207;
        v34[1] = "http";
      }
LABEL_17:
      sceUriBuild(0, v40, 0, v34, v15);
      v16 = v10 + v40[0];
      if ( a3 == 0 )
      {
        v33 = sceNet_lib_D8722983(a2);
        sceUriBuild(a2 + v33, 0, v40[0], v34, v15);
      }
      v17 = v16 + 1;
      if ( a3 == 0 )
        sceNet_lib_5705F6F9(a2, " ");
      v18 = v17 + sceNet_lib_D8722983("HTTP/D.D");
      if ( a3 == 0 )
      {
        v19 = "HTTP/1.0";
        if ( *(u32 *)(a1 + 140) != 0 )
          v19 = "HTTP/1.1";
        sceNet_lib_5705F6F9(a2, v19);
      }
      v20 = v18 + 2;
      if ( a3 == 0 )
        sceNet_lib_5705F6F9(a2, "\r\n");
      v21 = *(u32 **)(a1 + 288);
      if ( v21 == NULL )
      {
        v22 = *(u32 *)(v6 + 252);
        goto LABEL_43;
      }
      v23 = *(u32 *)(a1 + 28);
      break;
    default:
      return v7;
  }
  while ( 1 )
  {
    if ( v23 == 7 )
    {
      if ( sceNet_lib_9CFBC7E3(*v21, "Host") != 0 && sceNet_lib_9CFBC7E3(*v21, "Proxy-Authorization") != 0 )
      {
        v21 = (u32 *)v21[2];
        goto LABEL_40;
      }
    }
    else if ( *(u32 *)(v6 + 4) == 1 && sceNet_lib_9CFBC7E3(*v21, "Proxy-Authorization") == 0 )
    {
      v21 = (u32 *)v21[2];
      goto LABEL_40;
    }
    v24 = v20 + sceNet_lib_D8722983(*v21);
    if ( a3 == 0 )
      sceNet_lib_5705F6F9(a2, *v21);
    v25 = v24 + 2;
    if ( a3 == 0 )
      sceNet_lib_5705F6F9(a2, ": ");
    v26 = v21[1];
    if ( v26 != 0 )
    {
      v25 += sceNet_lib_D8722983(v26);
      if ( a3 == 0 )
        sceNet_lib_5705F6F9(a2, v21[1]);
    }
    v20 = v25 + 2;
    if ( a3 == 0 )
      sceNet_lib_5705F6F9(a2, "\r\n");
    v21 = (u32 *)v21[2];
LABEL_40:
    if ( v21 == NULL )
      break;
    v23 = *(u32 *)(a1 + 28);
  }
  v22 = *(u32 *)(v6 + 252);
LABEL_43:
  if ( v22 != 0 )
  {
    if ( *(u32 *)(v6 + 192) == 1 && (*(u32 *)(v6 + 4) == 0 || *(u32 *)(a1 + 28) == 7) )
      v27 = "Proxy-Connection: Keep-Alive";
    else
      v27 = "Connection: Keep-Alive";
  }
  else if ( *(u32 *)(v6 + 192) == 1 && (*(u32 *)(v6 + 4) == 0 || *(u32 *)(a1 + 28) == 7) )
  {
    v27 = "Proxy-Connection: close";
  }
  else
  {
    v27 = "Connection: close";
  }
  v28 = v20 + sceNet_lib_D8722983(v27);
  if ( a3 == 0 )
    sceNet_lib_5705F6F9(a2, v27);
  v29 = v28 + 2;
  if ( a3 == 0 )
    sceNet_lib_5705F6F9(a2, "\r\n");
  if ( *(u32 *)(a1 + 28) == 1 || *(u32 *)(a1 + 28) == 4 )
  {
    v31 = v29 + sceNet_lib_D8722983("Content-Length: ");
    if ( a3 == 0 )
      sceNet_lib_5705F6F9(a2, "Content-Length: ");
    sceKernelMemset(v39, 0, 21);
    http_io_fn_41(v39, *(u64 *)a1);
    v32 = v31 + sceNet_lib_D8722983(v39);
    if ( a3 == 0 )
      sceNet_lib_5705F6F9(a2, v39);
    v29 = v32 + 2;
    if ( a3 == 0 )
      sceNet_lib_5705F6F9(a2, "\r\n");
  }
  v7 = v29 + 3;
  if ( a3 != 0 )
    return v7;
  sceNet_lib_5705F6F9(a2, "\r\n");
  return v7;
}


int http_io_fn_81(int **a1, u32 *a2)
{
  u32 *v2;
  int v4;
  int *v5;
  int result;
  int v7;
  int *v8;
  int v9;
  int v10;
  int v11;
  int v12;
  u32 *v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;

  v2 = a2;
  v4 = 0;
  if ( a2 == NULL )
  {
    *a1 = NULL;
    return 0;
  }
  v5 = (int *)http_opts_fn_2(12);
  result = -2143088606;
  if ( v5 != NULL )
  {
    sceKernelMemset(v5, 0, 12);
    v8 = v5;
    v11 = http_io_fn_89(v7);
    while ( 1 )
    {
      if ( *v2 != 0 )
      {
        v17 = sceNet_lib_D8722983(*v2);
        v18 = http_opts_fn_2(v17 + 1);
        *v8 = v18;
        if ( v18 == 0 )
        {
LABEL_16:
          v4 = -2143088606;
LABEL_10:
          http_io_fn_90(v11, v9, v10);
          if ( v4 < 0 )
          {
            http_io_fn_79(v5);
            return v4;
          }
          else
          {
            result = v4;
            *a1 = v5;
          }
          return result;
        }
        sceNet_lib_80C9F02A(v18, *v2);
        v12 = v2[1];
      }
      else
      {
        v12 = v2[1];
      }
      if ( v12 != 0 )
      {
        v15 = sceNet_lib_D8722983(v12);
        v16 = http_opts_fn_2(v15 + 1);
        v8[1] = v16;
        if ( v16 == 0 )
          goto LABEL_16;
        sceNet_lib_80C9F02A(v16, v2[1]);
        v13 = (u32 *)v2[2];
      }
      else
      {
        v13 = (u32 *)v2[2];
      }
      if ( v13 != NULL )
      {
        v14 = http_opts_fn_2(12);
        v8[2] = v14;
        if ( v14 == 0 )
          goto LABEL_16;
        sceKernelMemset(v14, 0, 12);
        v13 = (u32 *)v2[2];
      }
      v2 = v13;
      v8 = (int *)v8[2];
      if ( v13 == NULL )
        goto LABEL_10;
    }
  }
  return result;
}


int http_io_fn_82(int a1, int a2, int a3)
{
  int result;

  result = sceParseHttpStatusLine(a1, a2, a3 + 20, a3 + 24, a3 + 28, a3 + 32, a3 + 36);
  if ( result >= 0 )
  {
    sceParseHttpResponseHeader(a1, a2, "Date", a3 + 56, a3 + 60);
    sceParseHttpResponseHeader(a1, a2, "Server", a3 + 48, a3 + 52);
    sceParseHttpResponseHeader(a1, a2, "ETag", a3 + 40, a3 + 44);
    sceParseHttpResponseHeader(a1, a2, "Last-Modifiled", a3 + 64, a3 + 68);
    sceParseHttpResponseHeader(a1, a2, "Location", a3 + 72, a3 + 76);
    sceParseHttpResponseHeader(a1, a2, "WWW-Authenticate", a3 + 80, a3 + 84);
    return 0;
  }
  return result;
}


int http_io_fn_83(int a1, u32 *a2)
{
  int result;
  int v5;
  int v6;
  int v7;
  unsigned int v8;
  s64 v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  char *v15;
  int v16;
  int v17;
  u32 *v18;
  int v19;
  u32 *v20;
  int v21;
  int v22;
  u8 v23[32];
  int v24;
  int v25;
  int v26;
  u8 v27[4];

  do
  {
    result = http_io_fn_39(a1);
    if ( result < 0 )
      return result;
    result = http_io_fn_82(*(u32 *)(a1 + 124), *(u32 *)(a1 + 68), a1 + 832);
    if ( result < 0 )
      return result;
    v5 = *(u32 *)(a1 + 124);
    v6 = *(u32 *)(a1 + 68);
    v25 = 0;
    v24 = 0;
    v7 = sceParseHttpResponseHeader(v5, v6, "Content-Length", &v24, &v25);
    v8 = v25;
    if ( v7 <= 0 )
    {
      *(u32 *)(a1 + 840) = 0;
    }
    else
    {
      *(u32 *)(a1 + 840) = 1;
      *(u32 *)(a1 + 844) = 0;
      if ( v8 >= 0x16 )
      {
        v8 = 21;
        v25 = 21;
      }
      sceKernelMemcpy(v23, v24, v8);
      v9 = ((u64)(v9) & 0xFFFFFFFF00000000ULL) | (u32)(v23);
      v9 = ((u64)(v9) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(&v23[v25]) << 32);
      *(u8 *)HIDWORD(v9) = 0;
      *(u32 *)(a1 + 832) = http_io_fn_40(v9);
      *(u32 *)(a1 + 836) = v10;
    }
    if ( sceParseHttpResponseHeader(*(u32 *)(a1 + 124), *(u32 *)(a1 + 68), "Transfer-Encoding", &v26, v27) <= 0 )
    {
      *(u32 *)(a1 + 844) = 0;
    }
    else
    {
      v11 = sceNet_lib_D8722983("chunked");
      if ( sceNet_lib_B9085A96(v26, "chunked", v11) != 0 )
      {
        *(u32 *)(a1 + 844) = 0;
      }
      else
      {
        *(u32 *)(a1 + 844) = 1;
        *(u32 *)(a1 + 832) = 0;
        *(u32 *)(a1 + 836) = 0;
        *(u32 *)(a1 + 840) = 0;
      }
    }
    v12 = a2[48];
    *(u32 *)(a1 + 848) = 0;
    a2[18] = 0;
    if ( v12 != 1 )
    {
      v13 = *(u32 *)(a1 + 124);
LABEL_13:
      v14 = *(u32 *)(a1 + 68);
      v15 = "Connection";
      goto LABEL_14;
    }
    if ( a2[1] != 0 && *(u32 *)(a1 + 28) != 7 )
    {
      v13 = *(u32 *)(a1 + 124);
      goto LABEL_13;
    }
    v13 = *(u32 *)(a1 + 124);
    v14 = *(u32 *)(a1 + 68);
    v15 = "Proxy-Connection";
LABEL_14:
    if ( sceParseHttpResponseHeader(v13, v14, v15, a1 + 920, a1 + 924) >= 0 )
    {
      if ( sceNet_lib_B9085A96(*(u32 *)(a1 + 920), "close", *(u32 *)(a1 + 924)) != 0 )
      {
        v16 = *(u32 *)(a1 + 860);
        continue;
      }
      *(u32 *)(a1 + 848) = 1;
      goto LABEL_18;
    }
    if ( a2[48] != 0 )
    {
      if ( a2[1] != 1 )
      {
        *(u32 *)(a1 + 848) = 1;
LABEL_18:
        a2[18] = 1;
LABEL_19:
        v16 = *(u32 *)(a1 + 860);
        continue;
      }
      v22 = *(u32 *)(a1 + 852);
    }
    else
    {
      v22 = *(u32 *)(a1 + 852);
    }
    if ( v22 != 1 )
    {
      *(u32 *)(a1 + 848) = 1;
LABEL_49:
      a2[18] = 1;
      goto LABEL_19;
    }
    if ( *(u32 *)(a1 + 856) != 1 )
    {
      *(u32 *)(a1 + 848) = 1;
      goto LABEL_49;
    }
    v16 = *(u32 *)(a1 + 860);
  }
  while ( (unsigned int)(v16 - 100) < 3 );
  if ( *(u32 *)(a1 + 224) != 0 )
  {
    v18 = *(u32 **)(a1 + 944);
    if ( v18 == NULL )
    {
      v17 = *(u16 *)(a1 + 310);
      goto LABEL_29;
    }
    if ( v16 == 401 )
    {
      v19 = 1;
      if ( v18[2] == 0 )
      {
        http_cookie_fn_6(v18);
        *(u32 *)(a1 + 944) = 0;
        goto LABEL_28;
      }
    }
    else
    {
      v19 = 2;
      if ( v18[2] == 0 )
      {
        v18[2] = 2;
        http_cookie_fn_3((int)v18);
        *(u32 *)(a1 + 944) = 0;
LABEL_28:
        v17 = *(u16 *)(a1 + 310);
        goto LABEL_29;
      }
    }
    v18[2] = v19;
    http_cookie_fn_5((int)v18);
    *(u32 *)(a1 + 944) = 0;
    goto LABEL_28;
  }
  v17 = *(u16 *)(a1 + 310);
LABEL_29:
  result = 0;
  if ( v17 != 0 )
  {
    v20 = *(u32 **)(a1 + 948);
    if ( v20 != NULL )
    {
      if ( *(u32 *)(a1 + 860) == 407 )
      {
        v21 = 1;
        if ( v20[2] == 0 )
        {
          http_cookie_fn_6(v20);
          *(u32 *)(a1 + 948) = 0;
          return 0;
        }
      }
      else
      {
        v21 = 2;
        if ( v20[2] == 0 )
        {
          v20[2] = 2;
          http_cookie_fn_3((int)v20);
          *(u32 *)(a1 + 948) = 0;
          return 0;
        }
      }
      v20[2] = v21;
      http_cookie_fn_5((int)v20);
      *(u32 *)(a1 + 948) = 0;
      return 0;
    }
  }
  return result;
}


int http_io_fn_84(int a1)
{
  int result;
  int v3;
  int v4;
  int v5;
  u32 *v6;
  int v7;
  int v8;

  result = -2143088130;
  if ( a1 == 0 )
    return result;
  v3 = *(u32 *)(a1 + 932);
  if ( v3 == 0 )
    v3 = *(u32 *)(a1 + 928);
  result = http_io_fn_83(a1, (u32 *)v3);
  if ( result < 0 )
    return result;
  v4 = *(u32 *)(a1 + 28);
  *(u32 *)(a1 + 60) = 4;
  if ( v4 == 2 )
    goto LABEL_10;
  if ( *(u32 *)(a1 + 840) == 0 )
  {
    v5 = *(u32 *)(a1 + 220);
    goto LABEL_13;
  }
  if ( *(u64 *)(a1 + 832) == 0 )
  {
LABEL_10:
    --*(u32 *)(v3 + 12);
    *(u32 *)(v3 + 16) = *(u32 *)(a1 + 44);
    if ( *(u32 *)(a1 + 848) == 1 )
      http_io_fn_24((u32 *)v3);
    *(u32 *)(a1 + 60) = 5;
    v5 = *(u32 *)(a1 + 220);
  }
  else
  {
    v5 = *(u32 *)(a1 + 220);
  }
LABEL_13:
  if ( v5 != 0 )
    http_cookie_fn_21(
      *(u32 *)(a1 + 124),
      *(u32 *)(a1 + 68),
      *(u32 *)(v3 + 56),
      *(u32 *)(a1 + 112),
      *(u16 *)(v3 + 400),
      a1);
  result = http_ssl_fn_25(a1);
  if ( result >= 0 )
  {
    if ( *(u32 *)(a1 + 60) == 3 )
      return *(u32 *)(a1 + 68);
    if ( *(u32 *)(a1 + 156) == 0 )
      return *(u32 *)(a1 + 68);
    if ( *(u32 *)(a1 + 64) != 0 )
      return *(u32 *)(a1 + 68);
    v6 = *(u32 **)(a1 + 936);
    if ( v6 == NULL )
    {
      v7 = *(u32 *)(a1 + 860);
      goto LABEL_26;
    }
    if ( *(u32 *)(a1 + 860) != 304 )
    {
      http_io_fn_65(v6);
      *(u32 *)(a1 + 936) = 0;
      v7 = *(u32 *)(a1 + 860);
LABEL_26:
      if ( v7 == 200
        && *(u32 *)(a1 + 28) == 0
        && http_io_fn_85(*(u32 *)(a1 + 288), "Range") == 0
        && http_io_fn_85(*(u32 *)(a1 + 288), "Content-Range") == 0 )
      {
        v8 = http_io_fn_50((u32 *)a1);
        *(u32 *)(a1 + 940) = v8;
        if ( v8 != 0 )
        {
          if ( http_io_fn_54((int *)(*(u32 *)(v8 + 48) + 28), 0, *(u32 *)(a1 + 124), *(u32 *)(a1 + 68)) == *(u32 *)(a1 + 68) )
          {
            http_io_fn_96(*(u32 *)(*(u32 *)(a1 + 940) + 48));
          }
          else
          {
            http_io_fn_52(*(u32 **)(a1 + 940));
            *(u32 *)(a1 + 940) = 0;
          }
        }
      }
      *(u32 *)(a1 + 64) = 1;
      return *(u32 *)(a1 + 68);
    }
    result = http_io_fn_74((u32 *)a1);
    if ( result >= 0 )
    {
      *(u32 *)(a1 + 64) = 2;
      *(u32 *)(a1 + 84) = 0;
      return *(u32 *)(a1 + 68);
    }
  }
  return result;
}


int http_io_fn_85(int *a1, int a2)
{
  int *v3;
  int v4;
  int result;
  int i;

  v3 = a1;
  v4 = 0;
  if ( a1 != NULL && a2 != 0 )
  {
    result = 0;
    if ( a1 == NULL )
      return result;
    for ( i = *a1; sceNet_lib_9CFBC7E3(i, a2) != 0; i = *v3 )
    {
      v3 = (int *)v3[2];
      if ( v3 == NULL )
        return v4;
    }
    return v3[1];
  }
  return v4;
}


int http_io_fn_86(int *a1, int a2, int a3, int a4)
{
  int v8;
  int v9;
  int v10;
  char *v11;
  int v12;
  u32 v14[8];
  s16 v15;
  int v16[4];

  v8 = 0;
  sceKernelMemset(v14, 0, 44);
  v16[0] = sceNet_lib_D8722983(a3) + 1;
  v9 = http_opts_fn_2(v16[0]);
  v10 = -2143088606;
  if ( v9 == 0 )
    goto LABEL_8;
  sceNet_lib_80C9F02A(v9, a3);
  v14[4] = v9;
  v11 = "https";
  if ( a2 != 1 )
    v11 = "http";
  v14[1] = v11;
  v15 = a4;
  v14[0] = 1;
  v10 = sceUriBuild(0, v16, 0, v14, 6);
  if ( v10 >= 0 && (v12 = http_opts_fn_2(v16[0]), v8 = v12, v12 != 0) && (v10 = sceUriBuild(v12, 0, v16[0], v14, 6)) >= 0 )
  {
    http_opts_fn_3(v9);
    *a1 = v8;
    return 0;
  }
  else
  {
LABEL_8:
    if ( v9 != 0 )
      http_opts_fn_3(v9);
    if ( v8 != 0 )
      http_opts_fn_3(v8);
    return v10;
  }
}


int http_io_fn_87(int *a1, int a2)
{
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;

  v4 = http_io_fn_89(a1);
  v5 = http_io_fn_80(a2, 0, 1);
  v6 = http_opts_fn_2(v5);
  v7 = v5;
  v8 = 0;
  v9 = 0;
  if ( v6 != 0 )
  {
    sceKernelMemset(v6, 0, v7);
    http_io_fn_80(a2, v6, 0);
  }
  else
  {
    v9 = -2143088606;
  }
  http_io_fn_90(v4, v8, v7);
  if ( v9 >= 0 )
  {
    *a1 = v6;
    return v9;
  }
  if ( v6 == 0 )
    return v9;
  http_opts_fn_3(v6);
  return v9;
}


int http_io_fn_88(u32 *a1)
{
  int v2;
  int v3;

  v2 = 0;
  v3 = http_opts_fn_2(a1[17] + 1);
  a1[32] = v3;
  if ( v3 == 0 )
    return -2143088606;
  sceKernelMemcpy(v3, a1[31], a1[17]);
  *(u8 *)(a1[32] + a1[17]) = 0;
  a1[23] = a1[17] + 1;
  return v2;
}


int http_io_fn_89()
{
  return sceKernelCpuSuspendIntr();
}


int http_io_fn_90()
{
  return sceKernelCpuResumeIntr();
}


int http_io_fn_91()
{
  return sceKernelUtilsMd5Digest();
}


int http_io_fn_92()
{
  return sceKernelUtilsMd5BlockInit();
}


int http_io_fn_93()
{
  return sceKernelUtilsMd5BlockUpdate();
}


int http_io_fn_94()
{
  return sceKernelUtilsMd5BlockResult();
}


int http_io_fn_95()
{
  return sceNetInetInetPton();
}


int http_io_fn_96()
{
  return sceRtcGetCurrentTick();
}


int http_io_fn_97()
{
  return sceRtcTickAddSeconds();
}


int http_io_fn_98()
{
  return sceRtcParseDateTime();
}


int http_io_fn_99()
{
  return sceRtcFormatRFC2822();
}


int http_io_fn_100(int a1)
{
  int v2;
  int *v3;
  int v4;
  u32 v6[4];

  v6[0] = 0;
  v2 = sceHttpStorage_driver_700AAD44(a1, 33555970, 420);
  v3 = v6;
  v4 = 4;
  if ( v2 < 0
    || (v2 = sceHttpStorage_driver_24AA94F4(a1, v6, 4), v3 = g_md5_empty_digest, v4 = 16, v2 < 0)
    || (v2 = sceHttpStorage_driver_24AA94F4(a1, g_md5_empty_digest, 16)) < 0 )
  {
    sceHttpStorage_driver_CDA3D8F6(a1, v3, v4);
    return v2;
  }
  else
  {
    sceHttpStorage_driver_CDA3D8F6(a1, v3, v4);
    return 0;
  }
}


int http_io_fn_101(int a1, int a2, int a3)
{
  int v4;
  int v5;
  int v6;
  int result;

  v5 = sceHttpStorage_driver_700AAD44(a1, a2, a3);
  v6 = 20;
  if ( v5 < 0 || (result = sceHttpStorage_driver_B33389CE(a1, v4, 20, 0, 0), v5 = result, result < 0) )
  {
    sceHttpStorage_driver_CDA3D8F6(a1, v4, v6);
    return v5;
  }
  return result;
}


int http_io_fn_102(int a1, unsigned int *a2)
{
  void *v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  unsigned int v10;
  int v11;
  int v12;
  int v13;
  int v15;
  u8 v16[16];
  u8 v17[8];
  int v18;
  int v19;
  unsigned int v20[4];

  if ( sceHttpStorage_driver_2D8DAE58(a1, v17) < 0 )
    goto LABEL_20;
  if ( sceHttpStorage_driver_700AAD44(a1, 1, 0) < 0 )
    goto LABEL_20;
  v6 = sceHttpStorage_driver_CDDF1103(a1, v20, 4);
  v4 = NULL;
  if ( v6 < 0 )
    goto LABEL_20;
  v5 = v18 - 20;
  v7 = v19 - 1 + ((unsigned int)(v18 - 20) < 0xFFFFFFEC);
  if ( v7 < 0 )
    goto LABEL_20;
  if ( v7 != 0 )
  {
    v8 = a1;
  }
  else
  {
    v4 = (void *)(v5 < v20[0]);
    v8 = a1;
    if ( v5 < v20[0] )
      goto LABEL_20;
  }
  if ( sceHttpStorage_driver_CDDF1103(v8, v16, 16) >= 0 && sceHttpStorage_driver_B33389CE(a1, v4, 20, 0, 0) >= 0 )
  {
    v9 = http_io_fn_92();
    v10 = v20[0];
    if ( v9 >= 0 )
    {
      if ( v20[0] != 0 )
      {
        v11 = v20[0] < 0x401;
        while ( 1 )
        {
          v12 = 1024;
          if ( v11 )
            v12 = v10;
          v10 -= v12;
          v5 = sceHttpStorage_driver_CDDF1103(a1, &HTTP_BSS_U8(0x3190), v12);
          v4 = &HTTP_BSS_U8(0x3190);
          if ( v5 < 0 || http_io_fn_93() < 0 )
            break;
          v11 = v10 < 0x401;
          if ( v10 == 0 )
            goto LABEL_17;
        }
      }
      else
      {
LABEL_17:
        if ( http_io_fn_94() >= 0 )
        {
          v13 = sceNet_lib_E0A81C7C(v16, &v15, 16);
          v5 = v20[0];
          if ( v13 == 0 )
          {
            *a2 = v20[0];
            sceHttpStorage_driver_CDA3D8F6(a1, v4, v5);
            return 1;
          }
        }
      }
    }
  }
LABEL_20:
  sceHttpStorage_driver_CDA3D8F6(a1, v4, v5);
  return 0;
}


int http_io_fn_103(int a1, unsigned int a2)
{
  void *v3;
  unsigned int v4;
  int v5;
  int v6;
  int v7;
  unsigned int v8;
  int v9;
  int v10;
  int v12;
  u8 v13[8];
  int v14;
  int v15;
  unsigned int v16[4];

  v16[0] = a2;
  v5 = sceHttpStorage_driver_2D8DAE58(a1, v13);
  if ( v5 < 0 )
    goto LABEL_19;
  v3 = NULL;
  v4 = v14 - 20;
  v6 = v15 - 1 + ((unsigned int)(v14 - 20) < 0xFFFFFFEC);
  if ( v6 < 0 )
    goto LABEL_21;
  v3 = (void *)(v4 < v16[0]);
  if ( v6 == 0 )
  {
    v7 = a1;
    if ( v4 >= v16[0] )
      goto LABEL_5;
LABEL_21:
    v5 = -1;
    goto LABEL_19;
  }
  v7 = a1;
LABEL_5:
  v5 = sceHttpStorage_driver_700AAD44(v7, 3, 0);
  if ( v5 >= 0 )
  {
    v5 = sceHttpStorage_driver_B33389CE(a1, v3, 20, 0, 0);
    if ( v5 >= 0 )
    {
      v5 = http_io_fn_92();
      if ( v5 >= 0 )
      {
        v8 = v16[0];
        if ( v16[0] != 0 )
        {
          v9 = v16[0] < 0x401;
          while ( 1 )
          {
            v10 = 1024;
            if ( v9 )
              v10 = v8;
            v8 -= v10;
            v5 = sceHttpStorage_driver_CDDF1103(a1, &HTTP_BSS_U8(0x3190), v10);
            v4 = v5;
            v3 = &HTTP_BSS_U8(0x3190);
            if ( v5 < 0 )
              break;
            v5 = http_io_fn_93();
            if ( v5 < 0 )
              break;
            v9 = v8 < 0x401;
            if ( v8 == 0 )
              goto LABEL_15;
          }
        }
        else
        {
LABEL_15:
          v5 = http_io_fn_94();
          if ( v5 >= 0 )
          {
            v5 = sceHttpStorage_driver_B33389CE(a1, v3, 0, 0, 0);
            if ( v5 >= 0 )
            {
              v5 = sceHttpStorage_driver_24AA94F4(a1, v16, 4);
              if ( v5 >= 0 )
                v5 = sceHttpStorage_driver_24AA94F4(a1, &v12, 16);
            }
          }
        }
      }
    }
  }
LABEL_19:
  sceHttpStorage_driver_CDA3D8F6(a1, v3, v4);
  return v5;
}


int http_io_fn_104()
{
  int v0;

  v0 = 0;
  http_io_fn_89();
  if ( HTTP_BSS_U32(0x318C) <= 0 )
    HTTP_BSS_U32(0x318C) = 1;
  else
    v0 = -2143088607;
  http_io_fn_90();
  return v0;
}


int http_io_fn_105()
{
  http_io_fn_89();
  HTTP_BSS_U32(0x318C) = 0;
  http_io_fn_90();
  return 0;
}


int http_io_fn_106(int a1, int a2, int a3)
{
  return sceHttpStorage_driver_CDA3D8F6(a1, a2, a3);
}


int http_io_fn_107(int a1, int a2, int a3)
{
  int result;

  result = sceHttpStorage_driver_CDDF1103(a1, a2, a3);
  if ( result >= 0 && result != a3 )
    return -1;
  return result;
}


int http_io_fn_108(int a1, int a2, int a3)
{
  int result;

  result = sceHttpStorage_driver_24AA94F4(a1, a2, a3);
  if ( result >= 0 && result != a3 )
    return -1;
  return result;
}


int http_io_fn_109(int a1, int a2, int a3, int a4, int a5)
{
  return sceHttpStorage_driver_B33389CE(a1, a2, a3, a4, a5);
}


