#include "libhttp_internal.h"

int http_opts_fn_1(int a1, u8 *a2)
{
  int result;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  u8 *v10;
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

  sceKernelMemset(&v20, 0, 44);
  result = 0;
  if ( a1 != 0 && a2 != NULL )
  {
    if ( sceNet_lib_D8722983(a2) != 0 && *a2 == 47 )
    {
      v18 = sceNet_lib_D8722983(a2) + 1;
      v19 = http_opts_fn_2(v18);
      v9 = v19;
      if ( v19 == 0 )
        return 0;
      sceKernelMemcpy(v19, a2, v18);
    }
    else
    {
      v5 = sceNet_lib_F48963C6(a1, 47);
      if ( v5 != 0 )
      {
        v7 = v5 - a1 + 1;
        v6 = sceNet_lib_D8722983(a2);
        v8 = http_opts_fn_2(v7 + v6 + 1);
        v9 = v8;
        if ( v8 == 0 )
          return 0;
        sceKernelMemcpy(v8, a1, v7);
        *(u8 *)(v9 + v7) = 0;
        v10 = a2;
        v11 = v9;
      }
      else
      {
        v16 = sceNet_lib_D8722983(a1);
        v15 = sceNet_lib_D8722983(a2);
        v17 = http_opts_fn_2(v16 + v15 + 1);
        v9 = v17;
        if ( v17 == 0 )
          return 0;
        sceNet_lib_80C9F02A(v17, a1);
        v11 = v9;
        v10 = a2;
      }
      sceNet_lib_5705F6F9(v11, v10);
    }
    v12 = sceNet_lib_D8722983(v9);
    v13 = http_opts_fn_2(v12 + 1);
    if ( v13 != 0 )
    {
      v14 = sceNet_lib_D8722983(v9);
      if ( sceUriSweepPath(v13, v9, v14 + 1) >= 0 )
      {
        result = v13;
        if ( v9 != 0 )
        {
          http_opts_fn_3(v9);
          return v13;
        }
        return result;
      }
    }
    if ( v9 != 0 )
      http_opts_fn_3(v9);
    if ( v13 == 0 )
      return 0;
    http_opts_fn_3(v13);
    return 0;
  }
  return result;
}


int http_opts_fn_2(int a1)
{
  int result;
  u32 v2[4];

  if ( HTTP_BSS_U32(0x2668) != NULL )
  {
    v2[0] = ((int (*)())HTTP_BSS_U32(0x2668))(a1, a1, v2);
  }
  else if ( sceKernelTryAllocateVpl(HTTP_BSS_U32(0x2674), a1, v2) < 0 )
  {
    v2[0] = 0;
  }
  result = v2[0];
  if ( v2[0] != 0 )
    ++HTTP_BSS_U32(0x267C);
  return result;
}


int http_opts_fn_3(int a1)
{
  if ( a1 != 0 )
    --HTTP_BSS_U32(0x267C);
  if ( HTTP_BSS_U32(0x266C) != NULL )
    return ((int (*)())HTTP_BSS_U32(0x266C))();
  else
    return sceKernelFreeVpl(HTTP_BSS_U32(0x2674), a1, 0x20000u, 0x20000u);
}


int sceHttpSetMallocFunction(int (*a1)(u32, u32, u32), int (*a2)(void), int a3)
{
  int v6; // dc
  int result;

  v6 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v6 )
  {
    result = 0;
    if ( a1 == NULL || a2 == NULL || a3 == 0 )
      return -2143088130;
    if ( HTTP_BSS_U32(0x267C) > 0 )
      return -2143088607;
    if ( HTTP_BSS_U32(0x2674) == 0 )
    {
LABEL_8:
      HTTP_BSS_U32(0x2668) = a1;
      HTTP_BSS_U32(0x2670) = a3;
      HTTP_BSS_U32(0x266C) = a2;
      return result;
    }
    result = sceKernelDeleteVpl();
    if ( result >= 0 )
    {
      HTTP_BSS_U32(0x2674) = 0;
      goto LABEL_8;
    }
  }
  return result;
}


int http_opts_fn_4(int a1)
{
  int MemoryInfo;
  u32 v4[4];

  if ( a1 == 0 )
    return 0;
  MemoryInfo = sceKernelQueryMemoryInfo(&HTTP_BSS_U32(0x2674), v4, 0);
  if ( MemoryInfo >= 0 )
  {
    MemoryInfo = sceKernelCreateVpl("SceHttp", v4[0], 0, a1, 0);
    if ( MemoryInfo >= 0 )
    {
      HTTP_BSS_U32(0x2674) = MemoryInfo;
      HTTP_BSS_U32(0x2678) = a1;
      return 0;
    }
  }
  if ( HTTP_BSS_U32(0x2674) != 0 )
  {
    sceKernelDeleteVpl();
    HTTP_BSS_U32(0x2674) = 0;
  }
  return MemoryInfo;
}


int http_opts_fn_5()
{
  int result;

  if ( HTTP_BSS_U32(0x2674) != 0 )
  {
    result = sceKernelDeleteVpl();
    HTTP_BSS_U32(0x2674) = 0;
    HTTP_BSS_U32(0x2678) = 0;
  }
  return result;
}


int http_opts_fn_6()
{
  int result;

  result = -1;
  if ( HTTP_BSS_U32(0x2668) == NULL )
    return HTTP_BSS_U32(0x2678);
  return result;
}


int sceHttpSetResolveRetry(int a1, int a2)
{
  int v4; // dc
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 v10[4];

  v4 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v4 )
  {
    v10[0] = 0;
    v6 = -2143088639;
    if ( http_opts_fn_23() == 1 && (v6 = http_io_fn_31(a1, v10, 1)) >= 0 )
    {
      *(u32 *)(v10[0] + 12) = a2;
      v7 = http_io_fn_89();
      v8 = v10[0];
      --*(u32 *)(v10[0] + 8);
      http_io_fn_90(v7, v8, v9);
      return v6;
    }
    else
    {
      return v6;
    }
  }
  return result;
}


int sceHttpSetResolveTimeOut(int a1, int a2)
{
  int v4; // dc
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 v10[4];

  v4 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v4 )
  {
    v10[0] = 0;
    v6 = -2143088639;
    if ( http_opts_fn_23() == 1 && (v6 = http_io_fn_31(a1, v10, 1)) >= 0 )
    {
      *(u32 *)(v10[0] + 24) = a2;
      v7 = http_io_fn_89();
      v8 = v10[0];
      --*(u32 *)(v10[0] + 8);
      http_io_fn_90(v7, v8, v9);
      return v6;
    }
    else
    {
      return v6;
    }
  }
  return result;
}


int sceHttpSetConnectTimeOut(int a1, int a2)
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
  int v13;
  u32 v14[3];

  v4 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v4 )
  {
    v13 = 0;
    v6 = -2143088639;
    v14[0] = 0;
    if ( http_opts_fn_23() == 1 )
    {
      v7 = http_io_fn_43(a1);
      v6 = v7;
      if ( v7 >= 0 )
      {
        if ( v7 != 0 )
        {
          v6 = -2143088384;
          if ( v7 == 0x1000000 )
          {
            v6 = http_io_fn_23(a1, v14, 0x1000000);
            if ( v6 >= 0 )
            {
              *(u32 *)(v14[0] + 204) = a2;
              v9 = http_io_fn_89();
              v12 = v14[0];
LABEL_10:
              --*(u32 *)(v12 + 8);
              http_io_fn_90(v9, v10, v11);
              return v6;
            }
          }
        }
        else
        {
          v6 = http_io_fn_31(a1, &v13, v8);
          if ( v6 >= 0 )
          {
            *(u32 *)(v13 + 28) = a2;
            v9 = http_io_fn_89();
            v12 = v13;
            goto LABEL_10;
          }
        }
      }
    }
    return v6;
  }
  return result;
}


int sceHttpSetSendTimeOut(int a1, int a2)
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
  int v13;
  u32 v14[3];

  v4 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v4 )
  {
    v13 = 0;
    v6 = -2143088639;
    v14[0] = 0;
    if ( http_opts_fn_23() == 1 )
    {
      v7 = http_io_fn_43(a1);
      v6 = v7;
      if ( v7 >= 0 )
      {
        if ( v7 != 0 )
        {
          v6 = -2143088384;
          if ( v7 == 0x1000000 )
          {
            v6 = http_io_fn_23(a1, v14, 0x1000000);
            if ( v6 >= 0 )
            {
              *(u32 *)(v14[0] + 212) = a2;
              v9 = http_io_fn_89();
              v12 = v14[0];
LABEL_10:
              --*(u32 *)(v12 + 8);
              http_io_fn_90(v9, v10, v11);
              return v6;
            }
          }
        }
        else
        {
          v6 = http_io_fn_31(a1, &v13, v8);
          if ( v6 >= 0 )
          {
            *(u32 *)(v13 + 36) = a2;
            v9 = http_io_fn_89();
            v12 = v13;
            goto LABEL_10;
          }
        }
      }
    }
    return v6;
  }
  return result;
}


int sceHttpSetRecvTimeOut(int a1, int a2)
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
  int v13;
  u32 v14[3];

  v4 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v4 )
  {
    v13 = 0;
    v6 = -2143088639;
    v14[0] = 0;
    if ( http_opts_fn_23() == 1 )
    {
      v7 = http_io_fn_43(a1);
      v6 = v7;
      if ( v7 >= 0 )
      {
        if ( v7 != 0 )
        {
          v6 = -2143088384;
          if ( v7 == 0x1000000 )
          {
            v6 = http_io_fn_23(a1, v14, 0x1000000);
            if ( v6 >= 0 )
            {
              *(u32 *)(v14[0] + 208) = a2;
              v9 = http_io_fn_89();
              v12 = v14[0];
LABEL_10:
              --*(u32 *)(v12 + 8);
              http_io_fn_90(v9, v10, v11);
              return v6;
            }
          }
        }
        else
        {
          v6 = http_io_fn_31(a1, &v13, v8);
          if ( v6 >= 0 )
          {
            *(u32 *)(v13 + 32) = a2;
            v9 = http_io_fn_89();
            v12 = v13;
            goto LABEL_10;
          }
        }
      }
    }
    return v6;
  }
  return result;
}


int sceHttp_CC920C12(int a1)
{
  int v2; // dc
  int result;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v12[3];

  v2 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v2 )
  {
    v11 = 0;
    v4 = -2143088639;
    v12[0] = 0;
    if ( http_opts_fn_23() == 1 )
    {
      v5 = http_io_fn_43(a1);
      v4 = v5;
      if ( v5 >= 0 )
      {
        if ( v5 != 0 )
        {
          v4 = -2143088384;
          if ( v5 == 0x1000000 )
          {
            v4 = http_io_fn_23(a1, v12, 0x1000000);
            if ( v4 >= 0 )
            {
              *(u32 *)(v12[0] + 268) = 1;
              v7 = http_io_fn_89();
              v10 = v12[0];
LABEL_10:
              --*(u32 *)(v10 + 8);
              http_io_fn_90(v7, v8, v9);
              return v4;
            }
          }
        }
        else
        {
          v4 = http_io_fn_31(a1, &v11, v6);
          if ( v4 >= 0 )
          {
            *(u32 *)(v11 + 92) = 1;
            v7 = http_io_fn_89();
            v10 = v11;
            goto LABEL_10;
          }
        }
      }
    }
    return v4;
  }
  return result;
}


int sceHttp_D29163DA(int a1)
{
  int v2; // dc
  int result;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v12[3];

  v2 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v2 )
  {
    v11 = 0;
    v4 = -2143088639;
    v12[0] = 0;
    if ( http_opts_fn_23() == 1 )
    {
      v5 = http_io_fn_43(a1);
      v4 = v5;
      if ( v5 >= 0 )
      {
        if ( v5 != 0 )
        {
          v4 = -2143088384;
          if ( v5 == 0x1000000 )
          {
            v4 = http_io_fn_23(a1, v12, 0x1000000);
            if ( v4 >= 0 )
            {
              *(u32 *)(v12[0] + 268) = 0;
              v7 = http_io_fn_89();
              v10 = v12[0];
LABEL_10:
              --*(u32 *)(v10 + 8);
              http_io_fn_90(v7, v8, v9);
              return v4;
            }
          }
        }
        else
        {
          v4 = http_io_fn_31(a1, &v11, v6);
          if ( v4 >= 0 )
          {
            *(u32 *)(v11 + 92) = 0;
            v7 = http_io_fn_89();
            v10 = v11;
            goto LABEL_10;
          }
        }
      }
    }
    return v4;
  }
  return result;
}


int sceHttpSetResHeaderMaxSize(int a1, int a2)
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
  int v13;
  u32 v14[3];

  v4 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v4 )
  {
    v13 = 0;
    v6 = -2143088639;
    v14[0] = 0;
    if ( http_opts_fn_23() == 1 )
    {
      v7 = http_io_fn_43(a1);
      v6 = v7;
      if ( v7 >= 0 )
      {
        if ( v7 != 0 )
        {
          v6 = -2143088384;
          if ( v7 == 0x1000000 )
          {
            v6 = http_io_fn_23(a1, v14, 0x1000000);
            if ( v6 >= 0 )
            {
              *(u32 *)(v14[0] + 216) = a2;
              v9 = http_io_fn_89();
              v12 = v14[0];
LABEL_10:
              --*(u32 *)(v12 + 8);
              http_io_fn_90(v9, v10, v11);
              return v6;
            }
          }
        }
        else
        {
          v6 = http_io_fn_31(a1, &v13, v8);
          if ( v6 >= 0 )
          {
            *(u32 *)(v13 + 40) = a2;
            v9 = http_io_fn_89();
            v12 = v13;
            goto LABEL_10;
          }
        }
      }
    }
    return v6;
  }
  return result;
}


int sceHttpSetRecvBlockSize(int a1, int a2)
{
  int v4; // dc
  int result;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v12[4];

  v4 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v4 )
  {
    v12[0] = 0;
    v6 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v7 = http_io_fn_43(a1);
      v6 = v7;
      if ( v7 >= 0 )
      {
        if ( v7 != 0 )
        {
          return -2143088384;
        }
        else
        {
          v6 = http_io_fn_31(a1, v12, v8);
          if ( v6 >= 0 )
          {
            *(u32 *)(v12[0] + 44) = a2;
            v9 = http_io_fn_89();
            v10 = v12[0];
            --*(u32 *)(v12[0] + 8);
            http_io_fn_90(v9, v11, v10);
            return v6;
          }
        }
      }
    }
    return v6;
  }
  return result;
}


int sceHttpSetCacheContentLengthMaxSize(int a1, int a2)
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
  int v13;
  int v14;
  int v15;

  v4 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v4 )
  {
    v13 = 0;
    v6 = -2143088639;
    v14 = 0;
    v15 = 0;
    if ( http_opts_fn_23() == 1 )
    {
      v7 = http_io_fn_43(a1);
      v6 = v7;
      if ( v7 >= 0 )
      {
        if ( v7 != 0 )
        {
          if ( v7 != 0x1000000 )
          {
            v6 = -2143088384;
            if ( v7 == 0x2000000 )
            {
              v6 = http_io_fn_12(a1, &v15);
              if ( v6 >= 0 )
              {
                *(u32 *)(v15 + 184) = a2;
                v8 = http_io_fn_89();
                --*(u32 *)(v15 + 32);
LABEL_11:
                http_io_fn_90(v8, v9, v10);
                return v6;
              }
            }
            return v6;
          }
          v6 = http_io_fn_23(a1, &v14, 0x1000000);
          if ( v6 >= 0 )
          {
            *(u32 *)(v14 + 224) = a2;
            v11 = http_io_fn_89();
            v12 = v14;
LABEL_14:
            v8 = v11;
            --*(u32 *)(v12 + 8);
            goto LABEL_11;
          }
        }
        else
        {
          v6 = http_io_fn_31(a1, &v13, 0x1000000);
          if ( v6 >= 0 )
          {
            *(u32 *)(v13 + 48) = a2;
            v11 = http_io_fn_89();
            v12 = v13;
            goto LABEL_14;
          }
        }
      }
    }
    return v6;
  }
  return result;
}


int sceHttpEnableKeepAlive(int a1)
{
  int v2; // dc
  int result;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v12[3];

  v2 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v2 )
  {
    v11 = 0;
    v4 = -2143088639;
    v12[0] = 0;
    if ( http_opts_fn_23() == 1 )
    {
      v5 = http_io_fn_43(a1);
      v4 = v5;
      if ( v5 >= 0 )
      {
        if ( v5 != 0 )
        {
          v4 = -2143088384;
          if ( v5 == 0x1000000 )
          {
            v4 = http_io_fn_23(a1, v12, 0x1000000);
            if ( v4 >= 0 )
            {
              *(u32 *)(v12[0] + 252) = 1;
              v7 = http_io_fn_89();
              v10 = v12[0];
LABEL_10:
              --*(u32 *)(v10 + 8);
              http_io_fn_90(v7, v8, v9);
              return v4;
            }
          }
        }
        else
        {
          v4 = http_io_fn_31(a1, &v11, v6);
          if ( v4 >= 0 )
          {
            *(u32 *)(v11 + 76) = 1;
            v7 = http_io_fn_89();
            v10 = v11;
            goto LABEL_10;
          }
        }
      }
    }
    return v4;
  }
  return result;
}


int sceHttpDisableKeepAlive(int a1)
{
  int v2; // dc
  int result;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v12[3];

  v2 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v2 )
  {
    v11 = 0;
    v4 = -2143088639;
    v12[0] = 0;
    if ( http_opts_fn_23() == 1 )
    {
      v5 = http_io_fn_43(a1);
      v4 = v5;
      if ( v5 >= 0 )
      {
        if ( v5 != 0 )
        {
          v4 = -2143088384;
          if ( v5 == 0x1000000 )
          {
            v4 = http_io_fn_23(a1, v12, 0x1000000);
            if ( v4 >= 0 )
            {
              *(u32 *)(v12[0] + 252) = 0;
              v7 = http_io_fn_89();
              v10 = v12[0];
LABEL_10:
              --*(u32 *)(v10 + 8);
              http_io_fn_90(v7, v8, v9);
              return v4;
            }
          }
        }
        else
        {
          v4 = http_io_fn_31(a1, &v11, v6);
          if ( v4 >= 0 )
          {
            *(u32 *)(v11 + 76) = 0;
            v7 = http_io_fn_89();
            v10 = v11;
            goto LABEL_10;
          }
        }
      }
    }
    return v4;
  }
  return result;
}


int sceHttpEnableCookie(int a1)
{
  int v2; // dc
  int result;
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

  v2 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v2 )
  {
    v4 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v4 = http_io_fn_43(a1);
      if ( v4 >= 0 )
      {
        v6 = http_io_fn_89();
        v7 = v4 > 0x1000000;
        if ( v4 == 0x1000000 )
        {
          v16 = http_io_fn_23(a1, &v18, v5);
          v11 = v18;
          v9 = v16;
          if ( v18 == 0 )
            goto LABEL_10;
          v12 = *(u32 *)(v18 + 8);
          *(u32 *)(v18 + 260) = 1;
          v9 = 0;
        }
        else
        {
          v8 = 0x2000000;
          if ( v4 > 0x1000000 )
          {
            if ( v4 == 0x2000000 )
            {
              v13 = http_io_fn_12(a1, &v19);
              v14 = v19;
              v9 = v13;
              if ( v19 != 0 )
              {
                v15 = *(u32 *)(v19 + 32);
                *(u32 *)(v19 + 220) = 1;
                *(u32 *)(v14 + 32) = v15 - 1;
              }
              goto LABEL_10;
            }
            goto LABEL_9;
          }
          if ( v4 != 0 )
          {
LABEL_9:
            v9 = -2143088384;
LABEL_10:
            http_io_fn_90(v6, v7, v8);
            return v9;
          }
          v10 = http_io_fn_31(a1, &v17, 0x2000000);
          v11 = v17;
          v9 = v10;
          if ( v17 == 0 )
            goto LABEL_10;
          v12 = *(u32 *)(v17 + 8);
          v9 = 0;
          *(u32 *)(v17 + 84) = 1;
        }
        *(u32 *)(v11 + 8) = v12 - 1;
        goto LABEL_10;
      }
    }
    return v4;
  }
  return result;
}


int sceHttpDisableCookie(int a1)
{
  int v2; // dc
  int result;
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

  v2 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v2 )
  {
    v4 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v4 = http_io_fn_43(a1);
      if ( v4 >= 0 )
      {
        v6 = http_io_fn_89();
        v7 = v4 > 0x1000000;
        switch ( v4 )
        {
          case 0:
            v9 = http_io_fn_31(a1, &v16, 0);
            v10 = v16;
            v8 = v9;
            if ( v16 == 0 )
              goto LABEL_10;
            v11 = *(u32 *)(v16 + 8);
            v8 = 0;
            *(u32 *)(v16 + 84) = 0;
            break;
          case 0x1000000:
            v15 = http_io_fn_23(a1, &v17, v7);
            v10 = v17;
            v8 = v15;
            if ( v17 == 0 )
              goto LABEL_10;
            v11 = *(u32 *)(v17 + 8);
            *(u32 *)(v17 + 260) = 0;
            v8 = 0;
            break;
          case 0x2000000:
            v12 = http_io_fn_12(a1, &v18);
            v13 = v18;
            v8 = v12;
            if ( v18 != 0 )
            {
              v14 = *(u32 *)(v18 + 32);
              *(u32 *)(v18 + 220) = 0;
              *(u32 *)(v13 + 32) = v14 - 1;
            }
            goto LABEL_10;
          default:
            v8 = -2143088384;
LABEL_10:
            http_io_fn_90(v6, v5, v7);
            return v8;
        }
        *(u32 *)(v10 + 8) = v11 - 1;
        goto LABEL_10;
      }
    }
    return v4;
  }
  return result;
}


int sceHttpEnableAuth(int a1)
{
  int v2; // dc
  int result;
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

  v2 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v2 )
  {
    v4 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v4 = http_io_fn_43(a1);
      if ( v4 >= 0 )
      {
        v6 = http_io_fn_89();
        v7 = v4 > 0x1000000;
        if ( v4 == 0x1000000 )
        {
          v16 = http_io_fn_23(a1, &v18, v5);
          v11 = v18;
          v9 = v16;
          if ( v18 == 0 )
            goto LABEL_10;
          v12 = *(u32 *)(v18 + 8);
          *(u32 *)(v18 + 264) = 1;
          v9 = 0;
        }
        else
        {
          v8 = 0x2000000;
          if ( v4 > 0x1000000 )
          {
            if ( v4 == 0x2000000 )
            {
              v13 = http_io_fn_12(a1, &v19);
              v14 = v19;
              v9 = v13;
              if ( v19 != 0 )
              {
                v15 = *(u32 *)(v19 + 32);
                *(u32 *)(v19 + 224) = 1;
                *(u32 *)(v14 + 32) = v15 - 1;
              }
              goto LABEL_10;
            }
            goto LABEL_9;
          }
          if ( v4 != 0 )
          {
LABEL_9:
            v9 = -2143088384;
LABEL_10:
            http_io_fn_90(v6, v7, v8);
            return v9;
          }
          v10 = http_io_fn_31(a1, &v17, 0x2000000);
          v11 = v17;
          v9 = v10;
          if ( v17 == 0 )
            goto LABEL_10;
          v12 = *(u32 *)(v17 + 8);
          v9 = 0;
          *(u32 *)(v17 + 88) = 1;
        }
        *(u32 *)(v11 + 8) = v12 - 1;
        goto LABEL_10;
      }
    }
    return v4;
  }
  return result;
}


int sceHttpDisableAuth(int a1)
{
  int v2; // dc
  int result;
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

  v2 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v2 )
  {
    v4 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v4 = http_io_fn_43(a1);
      if ( v4 >= 0 )
      {
        v6 = http_io_fn_89();
        v7 = v4 > 0x1000000;
        switch ( v4 )
        {
          case 0:
            v9 = http_io_fn_31(a1, &v16, 0);
            v10 = v16;
            v8 = v9;
            if ( v16 == 0 )
              goto LABEL_10;
            v11 = *(u32 *)(v16 + 8);
            v8 = 0;
            *(u32 *)(v16 + 88) = 0;
            break;
          case 0x1000000:
            v15 = http_io_fn_23(a1, &v17, v7);
            v10 = v17;
            v8 = v15;
            if ( v17 == 0 )
              goto LABEL_10;
            v11 = *(u32 *)(v17 + 8);
            *(u32 *)(v17 + 264) = 0;
            v8 = 0;
            break;
          case 0x2000000:
            v12 = http_io_fn_12(a1, &v18);
            v13 = v18;
            v8 = v12;
            if ( v18 != 0 )
            {
              v14 = *(u32 *)(v18 + 32);
              *(u32 *)(v18 + 224) = 0;
              *(u32 *)(v13 + 32) = v14 - 1;
            }
            goto LABEL_10;
          default:
            v8 = -2143088384;
LABEL_10:
            http_io_fn_90(v6, v5, v7);
            return v8;
        }
        *(u32 *)(v10 + 8) = v11 - 1;
        goto LABEL_10;
      }
    }
    return v4;
  }
  return result;
}


int sceHttp_CDB0DC58(int a1)
{
  int v2; // dc
  int result;
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

  v2 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v2 )
  {
    v4 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v4 = http_io_fn_43(a1);
      if ( v4 >= 0 )
      {
        v6 = http_io_fn_89();
        v7 = v4 > 0x1000000;
        if ( v4 == 0x1000000 )
        {
          v16 = http_io_fn_23(a1, &v18, v5);
          v11 = v18;
          v9 = v16;
          if ( v18 == 0 )
            goto LABEL_10;
          v12 = *(u32 *)(v18 + 8);
          *(u16 *)(v18 + 350) = 1;
          v9 = 0;
        }
        else
        {
          v8 = 0x2000000;
          if ( v4 > 0x1000000 )
          {
            if ( v4 == 0x2000000 )
            {
              v13 = http_io_fn_12(a1, &v19);
              v14 = v19;
              v9 = v13;
              if ( v19 != 0 )
              {
                v15 = *(u32 *)(v19 + 32);
                *(u16 *)(v19 + 310) = 1;
                *(u32 *)(v14 + 32) = v15 - 1;
              }
              goto LABEL_10;
            }
            goto LABEL_9;
          }
          if ( v4 != 0 )
          {
LABEL_9:
            v9 = -2143088384;
LABEL_10:
            http_io_fn_90(v6, v7, v8);
            return v9;
          }
          v10 = http_io_fn_31(a1, &v17, 0x2000000);
          v11 = v17;
          v9 = v10;
          if ( v17 == 0 )
            goto LABEL_10;
          v12 = *(u32 *)(v17 + 8);
          v9 = 0;
          *(u16 *)(v17 + 174) = 1;
        }
        *(u32 *)(v11 + 8) = v12 - 1;
        goto LABEL_10;
      }
    }
    return v4;
  }
  return result;
}


int sceHttp_951D310E(int a1)
{
  int v2; // dc
  int result;
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

  v2 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v2 )
  {
    v4 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v4 = http_io_fn_43(a1);
      if ( v4 >= 0 )
      {
        v6 = http_io_fn_89();
        v7 = v4 > 0x1000000;
        switch ( v4 )
        {
          case 0:
            v9 = http_io_fn_31(a1, &v16, 0);
            v10 = v16;
            v8 = v9;
            if ( v16 == 0 )
              goto LABEL_10;
            v11 = *(u32 *)(v16 + 8);
            v8 = 0;
            *(u16 *)(v16 + 174) = 0;
            break;
          case 0x1000000:
            v15 = http_io_fn_23(a1, &v17, v7);
            v10 = v17;
            v8 = v15;
            if ( v17 == 0 )
              goto LABEL_10;
            v11 = *(u32 *)(v17 + 8);
            *(u16 *)(v17 + 350) = 0;
            v8 = 0;
            break;
          case 0x2000000:
            v12 = http_io_fn_12(a1, &v18);
            v13 = v18;
            v8 = v12;
            if ( v18 != 0 )
            {
              v14 = *(u32 *)(v18 + 32);
              *(u16 *)(v18 + 310) = 0;
              *(u32 *)(v13 + 32) = v14 - 1;
            }
            goto LABEL_10;
          default:
            v8 = -2143088384;
LABEL_10:
            http_io_fn_90(v6, v5, v7);
            return v8;
        }
        *(u32 *)(v10 + 8) = v11 - 1;
        goto LABEL_10;
      }
    }
    return v4;
  }
  return result;
}


int sceHttp_D80BE761(int a1)
{
  int v2; // dc
  int result;
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

  v2 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v2 )
  {
    v4 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v4 = http_io_fn_43(a1);
      if ( v4 >= 0 )
      {
        v6 = http_io_fn_89();
        v7 = v4 > 0x1000000;
        if ( v4 == 0x1000000 )
        {
          v16 = http_io_fn_23(a1, &v18, v5);
          v11 = v18;
          v9 = v16;
          if ( v18 == 0 )
            goto LABEL_10;
          v12 = *(u32 *)(v18 + 8);
          *(u16 *)(v18 + 274) = 1;
          v9 = 0;
        }
        else
        {
          v8 = 0x2000000;
          if ( v4 > 0x1000000 )
          {
            if ( v4 == 0x2000000 )
            {
              v13 = http_io_fn_12(a1, &v19);
              v14 = v19;
              v9 = v13;
              if ( v19 != 0 )
              {
                v15 = *(u32 *)(v19 + 32);
                *(u16 *)(v19 + 234) = 1;
                *(u32 *)(v14 + 32) = v15 - 1;
              }
              goto LABEL_10;
            }
            goto LABEL_9;
          }
          if ( v4 != 0 )
          {
LABEL_9:
            v9 = -2143088384;
LABEL_10:
            http_io_fn_90(v6, v7, v8);
            return v9;
          }
          v10 = http_io_fn_31(a1, &v17, 0x2000000);
          v11 = v17;
          v9 = v10;
          if ( v17 == 0 )
            goto LABEL_10;
          v12 = *(u32 *)(v17 + 8);
          v9 = 0;
          *(u16 *)(v17 + 98) = 1;
        }
        *(u32 *)(v11 + 8) = v12 - 1;
        goto LABEL_10;
      }
    }
    return v4;
  }
  return result;
}


int sceHttp_C0E69162(int a1)
{
  int v2; // dc
  int result;
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

  v2 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v2 )
  {
    v4 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v4 = http_io_fn_43(a1);
      if ( v4 >= 0 )
      {
        v6 = http_io_fn_89();
        v7 = v4 > 0x1000000;
        switch ( v4 )
        {
          case 0:
            v9 = http_io_fn_31(a1, &v16, 0);
            v10 = v16;
            v8 = v9;
            if ( v16 == 0 )
              goto LABEL_10;
            v11 = *(u32 *)(v16 + 8);
            v8 = 0;
            *(u16 *)(v16 + 98) = 0;
            break;
          case 0x1000000:
            v15 = http_io_fn_23(a1, &v17, v7);
            v10 = v17;
            v8 = v15;
            if ( v17 == 0 )
              goto LABEL_10;
            v11 = *(u32 *)(v17 + 8);
            *(u16 *)(v17 + 274) = 0;
            v8 = 0;
            break;
          case 0x2000000:
            v12 = http_io_fn_12(a1, &v18);
            v13 = v18;
            v8 = v12;
            if ( v18 != 0 )
            {
              v14 = *(u32 *)(v18 + 32);
              *(u16 *)(v18 + 234) = 0;
              *(u32 *)(v13 + 32) = v14 - 1;
            }
            goto LABEL_10;
          default:
            v8 = -2143088384;
LABEL_10:
            http_io_fn_90(v6, v5, v7);
            return v8;
        }
        *(u32 *)(v10 + 8) = v11 - 1;
        goto LABEL_10;
      }
    }
    return v4;
  }
  return result;
}


int sceHttpEnableRedirect(int a1)
{
  int v2; // dc
  int result;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v12[3];

  v2 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v2 )
  {
    v11 = 0;
    v4 = -2143088639;
    v12[0] = 0;
    if ( http_opts_fn_23() == 1 )
    {
      v5 = http_io_fn_43(a1);
      v4 = v5;
      if ( v5 >= 0 )
      {
        if ( v5 != 0 )
        {
          v4 = -2143088384;
          if ( v5 == 0x1000000 )
          {
            v4 = http_io_fn_23(a1, v12, 0x1000000);
            if ( v4 >= 0 )
            {
              *(u32 *)(v12[0] + 256) = 1;
              v7 = http_io_fn_89();
              v10 = v12[0];
LABEL_10:
              --*(u32 *)(v10 + 8);
              http_io_fn_90(v7, v8, v9);
              return v4;
            }
          }
        }
        else
        {
          v4 = http_io_fn_31(a1, &v11, v6);
          if ( v4 >= 0 )
          {
            *(u32 *)(v11 + 80) = 1;
            v7 = http_io_fn_89();
            v10 = v11;
            goto LABEL_10;
          }
        }
      }
    }
    return v4;
  }
  return result;
}


int sceHttpDisableRedirect(int a1)
{
  int v2; // dc
  int result;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v12[3];

  v2 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v2 )
  {
    v11 = 0;
    v4 = -2143088639;
    v12[0] = 0;
    if ( http_opts_fn_23() == 1 )
    {
      v5 = http_io_fn_43(a1);
      v4 = v5;
      if ( v5 >= 0 )
      {
        if ( v5 != 0 )
        {
          v4 = -2143088384;
          if ( v5 == 0x1000000 )
          {
            v4 = http_io_fn_23(a1, v12, 0x1000000);
            if ( v4 >= 0 )
            {
              *(u32 *)(v12[0] + 256) = 0;
              v7 = http_io_fn_89();
              v10 = v12[0];
LABEL_10:
              --*(u32 *)(v10 + 8);
              http_io_fn_90(v7, v8, v9);
              return v4;
            }
          }
        }
        else
        {
          v4 = http_io_fn_31(a1, &v11, v6);
          if ( v4 >= 0 )
          {
            *(u32 *)(v11 + 80) = 0;
            v7 = http_io_fn_89();
            v10 = v11;
            goto LABEL_10;
          }
        }
      }
    }
    return v4;
  }
  return result;
}


int sceHttpChangeHttpVersion(int a1, int a2)
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
  int v13;
  int v14;
  int v15;

  v4 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v4 )
  {
    v13 = 0;
    v6 = -2143088639;
    v14 = 0;
    v15 = 0;
    if ( http_opts_fn_23() == 1 )
    {
      v7 = http_io_fn_43(a1);
      v6 = v7;
      if ( v7 >= 0 )
      {
        if ( v7 != 0 )
        {
          if ( v7 != 0x1000000 )
          {
            v6 = -2143088384;
            if ( v7 == 0x2000000 )
            {
              v6 = http_io_fn_12(a1, &v15);
              if ( v6 >= 0 )
              {
                *(u32 *)(v15 + 140) = a2;
                v8 = http_io_fn_89();
                --*(u32 *)(v15 + 32);
LABEL_11:
                http_io_fn_90(v8, v9, v10);
                return v6;
              }
            }
            return v6;
          }
          v6 = http_io_fn_23(a1, &v14, 0x1000000);
          if ( v6 >= 0 )
          {
            *(u32 *)(v14 + 256) = 0;
            v11 = http_io_fn_89();
            v12 = v14;
LABEL_14:
            v8 = v11;
            --*(u32 *)(v12 + 8);
            goto LABEL_11;
          }
        }
        else
        {
          v6 = http_io_fn_31(a1, &v13, 0x1000000);
          if ( v6 >= 0 )
          {
            *(u32 *)(v13 + 4) = a2;
            v11 = http_io_fn_89();
            v12 = v13;
            goto LABEL_14;
          }
        }
      }
    }
    return v6;
  }
  return result;
}


int sceHttpDisableCache(int a1)
{
  int v2; // dc
  int result;
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

  v2 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v2 )
  {
    v17 = 0;
    v18 = 0;
    if ( http_opts_fn_23() != 1 || http_io_fn_66() != 1 )
      return -2143088639;
    v4 = http_io_fn_43(a1);
    if ( v4 >= 0 )
    {
      v6 = http_io_fn_89();
      v7 = v4 > 0x1000000;
      if ( v4 == 0x1000000 )
      {
        v16 = http_io_fn_23(a1, &v18, v5);
        v11 = v18;
        v9 = v16;
        if ( v18 == 0 )
          goto LABEL_12;
        v12 = *(u32 *)(v18 + 8);
        *(u32 *)(v18 + 196) = 0;
        v9 = 0;
      }
      else
      {
        v8 = 0x2000000;
        if ( v4 > 0x1000000 )
        {
          if ( v4 == 0x2000000 )
          {
            v13 = http_io_fn_12(a1, &v19);
            v14 = v19;
            v9 = v13;
            if ( v19 != 0 )
            {
              v15 = *(u32 *)(v19 + 32);
              *(u32 *)(v19 + 156) = 0;
              *(u32 *)(v14 + 32) = v15 - 1;
            }
            goto LABEL_12;
          }
          goto LABEL_11;
        }
        if ( v4 != 0 )
        {
LABEL_11:
          v9 = -2143088384;
LABEL_12:
          http_io_fn_90(v6, v7, v8);
          return v9;
        }
        v10 = http_io_fn_31(a1, &v17, 0x2000000);
        v11 = v17;
        v9 = v10;
        if ( v17 == 0 )
          goto LABEL_12;
        v12 = *(u32 *)(v17 + 8);
        v9 = 0;
        *(u32 *)(v17 + 20) = 0;
      }
      *(u32 *)(v11 + 8) = v12 - 1;
      goto LABEL_12;
    }
    return v4;
  }
  return result;
}


int sceHttpSetAuthInfoCB(int a1, int a2)
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
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;

  v4 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v4 )
  {
    v6 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v6 = http_io_fn_43(a1);
      if ( v6 >= 0 )
      {
        v8 = http_io_fn_89();
        v9 = v6 > 0x1000000;
        if ( v6 == 0x1000000 )
        {
          http_io_fn_23(a1, &v17, v9);
          v11 = v17;
          v10 = -2143088384;
          if ( v17 == 0 )
            goto LABEL_10;
          v12 = *(u32 *)(v17 + 8);
          *(u32 *)(v17 + 284) = a2;
          v10 = 0;
        }
        else
        {
          if ( v6 > 0x1000000 )
          {
            if ( v6 == 0x2000000 )
            {
              v13 = http_io_fn_12(a1, &v18);
              v14 = v18;
              v10 = v13;
              if ( v18 != 0 )
              {
                v15 = *(u32 *)(v18 + 32);
                *(u32 *)(v18 + 244) = a2;
                *(u32 *)(v14 + 32) = v15 - 1;
                goto LABEL_10;
              }
            }
            goto LABEL_9;
          }
          if ( v6 != 0 )
          {
LABEL_9:
            v10 = -2143088384;
LABEL_10:
            http_io_fn_90(v8, v7, v9);
            return v10;
          }
          http_io_fn_31(a1, &v16, 0);
          v11 = v16;
          v10 = -2143088384;
          if ( v16 == 0 )
            goto LABEL_10;
          v12 = *(u32 *)(v16 + 8);
          v10 = 0;
          *(u32 *)(v16 + 108) = a2;
        }
        *(u32 *)(v11 + 8) = v12 - 1;
        goto LABEL_10;
      }
    }
    return v6;
  }
  return result;
}


int sceHttpSetAuthInfoCallback(int a1, int a2, int a3)
{
  int v6; // dc
  int result;
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

  v6 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v6 )
  {
    v8 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v8 = http_io_fn_43(a1);
      if ( v8 >= 0 )
      {
        v10 = http_io_fn_89();
        v11 = v8 > 0x1000000;
        if ( v8 == 0x1000000 )
        {
          http_io_fn_23(a1, &v21, v11);
          v18 = v21;
          v12 = -2143088384;
          if ( v21 != 0 )
          {
            v19 = *(u32 *)(v21 + 8);
            v12 = 0;
            *(u32 *)(v21 + 288) = a2;
            *(u32 *)(v18 + 8) = v19 - 1;
            *(u32 *)(v18 + 292) = a3;
          }
          goto LABEL_10;
        }
        if ( v8 > 0x1000000 )
        {
          if ( v8 == 0x2000000 )
          {
            v15 = http_io_fn_12(a1, &v22);
            v16 = v22;
            v12 = v15;
            if ( v22 != 0 )
            {
              v17 = *(u32 *)(v22 + 32);
              *(u32 *)(v22 + 248) = a2;
              *(u32 *)(v16 + 32) = v17 - 1;
              *(u32 *)(v16 + 252) = a3;
              goto LABEL_10;
            }
          }
        }
        else if ( v8 == 0 )
        {
          http_io_fn_31(a1, &v20, 0);
          v13 = v20;
          v12 = -2143088384;
          if ( v20 != 0 )
          {
            v14 = *(u32 *)(v20 + 8);
            v12 = 0;
            *(u32 *)(v20 + 112) = a2;
            *(u32 *)(v13 + 8) = v14 - 1;
            *(u32 *)(v13 + 116) = a3;
          }
          goto LABEL_10;
        }
        v12 = -2143088384;
LABEL_10:
        http_io_fn_90(v10, v9, v11);
        return v12;
      }
    }
    return v8;
  }
  return result;
}


int sceHttpSetRedirectCallback(int a1, int a2, int a3)
{
  int v6; // dc
  int result;
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

  v6 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v6 )
  {
    v8 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v8 = http_io_fn_43(a1);
      if ( v8 >= 0 )
      {
        v10 = http_io_fn_89();
        v11 = v8 > 0x1000000;
        if ( v8 == 0x1000000 )
        {
          http_io_fn_23(a1, &v21, v11);
          v18 = v21;
          v12 = -2143088384;
          if ( v21 != 0 )
          {
            v19 = *(u32 *)(v21 + 8);
            v12 = 0;
            *(u32 *)(v21 + 296) = a2;
            *(u32 *)(v18 + 8) = v19 - 1;
            *(u32 *)(v18 + 300) = a3;
          }
          goto LABEL_10;
        }
        if ( v8 > 0x1000000 )
        {
          if ( v8 == 0x2000000 )
          {
            v15 = http_io_fn_12(a1, &v22);
            v16 = v22;
            v12 = v15;
            if ( v22 != 0 )
            {
              v17 = *(u32 *)(v22 + 32);
              *(u32 *)(v22 + 256) = a2;
              *(u32 *)(v16 + 32) = v17 - 1;
              *(u32 *)(v16 + 260) = a3;
              goto LABEL_10;
            }
          }
        }
        else if ( v8 == 0 )
        {
          http_io_fn_31(a1, &v20, 0);
          v13 = v20;
          v12 = -2143088384;
          if ( v20 != 0 )
          {
            v14 = *(u32 *)(v20 + 8);
            v12 = 0;
            *(u32 *)(v20 + 120) = a2;
            *(u32 *)(v13 + 8) = v14 - 1;
            *(u32 *)(v13 + 124) = a3;
          }
          goto LABEL_10;
        }
        v12 = -2143088384;
LABEL_10:
        http_io_fn_90(v10, v9, v11);
        return v12;
      }
    }
    return v8;
  }
  return result;
}


int sceHttpSetCookieSendCallback(int a1, int a2, int a3)
{
  int v6; // dc
  int result;
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

  v6 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v6 )
  {
    v8 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v8 = http_io_fn_43(a1);
      if ( v8 >= 0 )
      {
        v10 = http_io_fn_89();
        v11 = v8 > 0x1000000;
        if ( v8 == 0x1000000 )
        {
          http_io_fn_23(a1, &v21, v11);
          v18 = v21;
          v12 = -2143088384;
          if ( v21 != 0 )
          {
            v19 = *(u32 *)(v21 + 8);
            v12 = 0;
            *(u32 *)(v21 + 304) = a2;
            *(u32 *)(v18 + 8) = v19 - 1;
            *(u32 *)(v18 + 308) = a3;
          }
          goto LABEL_10;
        }
        if ( v8 > 0x1000000 )
        {
          if ( v8 == 0x2000000 )
          {
            v15 = http_io_fn_12(a1, &v22);
            v16 = v22;
            v12 = v15;
            if ( v22 != 0 )
            {
              v17 = *(u32 *)(v22 + 32);
              *(u32 *)(v22 + 264) = a2;
              *(u32 *)(v16 + 32) = v17 - 1;
              *(u32 *)(v16 + 268) = a3;
              goto LABEL_10;
            }
          }
        }
        else if ( v8 == 0 )
        {
          http_io_fn_31(a1, &v20, 0);
          v13 = v20;
          v12 = -2143088384;
          if ( v20 != 0 )
          {
            v14 = *(u32 *)(v20 + 8);
            v12 = 0;
            *(u32 *)(v20 + 128) = a2;
            *(u32 *)(v13 + 8) = v14 - 1;
            *(u32 *)(v13 + 132) = a3;
          }
          goto LABEL_10;
        }
        v12 = -2143088384;
LABEL_10:
        http_io_fn_90(v10, v9, v11);
        return v12;
      }
    }
    return v8;
  }
  return result;
}


int sceHttpSetCookieRecvCallback(int a1, int a2, int a3)
{
  int v6; // dc
  int result;
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

  v6 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v6 )
  {
    v8 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      v8 = http_io_fn_43(a1);
      if ( v8 >= 0 )
      {
        v10 = http_io_fn_89();
        v11 = v8 > 0x1000000;
        if ( v8 == 0x1000000 )
        {
          http_io_fn_23(a1, &v21, v11);
          v18 = v21;
          v12 = -2143088384;
          if ( v21 != 0 )
          {
            v19 = *(u32 *)(v21 + 8);
            v12 = 0;
            *(u32 *)(v21 + 312) = a2;
            *(u32 *)(v18 + 8) = v19 - 1;
            *(u32 *)(v18 + 316) = a3;
          }
          goto LABEL_10;
        }
        if ( v8 > 0x1000000 )
        {
          if ( v8 == 0x2000000 )
          {
            v15 = http_io_fn_12(a1, &v22);
            v16 = v22;
            v12 = v15;
            if ( v22 != 0 )
            {
              v17 = *(u32 *)(v22 + 32);
              *(u32 *)(v22 + 272) = a2;
              *(u32 *)(v16 + 32) = v17 - 1;
              *(u32 *)(v16 + 276) = a3;
              goto LABEL_10;
            }
          }
        }
        else if ( v8 == 0 )
        {
          http_io_fn_31(a1, &v20, 0);
          v13 = v20;
          v12 = -2143088384;
          if ( v20 != 0 )
          {
            v14 = *(u32 *)(v20 + 8);
            v12 = 0;
            *(u32 *)(v20 + 136) = a2;
            *(u32 *)(v13 + 8) = v14 - 1;
            *(u32 *)(v13 + 140) = a3;
          }
          goto LABEL_10;
        }
        v12 = -2143088384;
LABEL_10:
        http_io_fn_90(v10, v9, v11);
        return v12;
      }
    }
    return v8;
  }
  return result;
}


int http_opts_fn_7(int a1, int a2, int a3, int a4)
{
  int v4;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int i;
  int v20;
  int v21;
  int v22;
  int v23;

  v4 = -2143088639;
  v21 = 0;
  v22 = 0;
  v23 = 0;
  v20 = 0;
  if ( http_opts_fn_23() != 1 )
    goto LABEL_2;
  if ( a4 == 0 )
    http_opts_fn_9(a1, a2);
  v4 = http_io_fn_78(&v20, a2, a3);
  if ( v4 < 0 )
    goto LABEL_2;
  v10 = http_io_fn_43(a1);
  v4 = v10;
  if ( v10 < 0 )
    goto LABEL_2;
  if ( v10 != 0 )
  {
    if ( v10 == 0x1000000 )
    {
      v4 = http_io_fn_23(a1, &v22, v11);
      if ( v4 >= 0 )
      {
        v12 = v22 + 176;
        goto LABEL_12;
      }
    }
    else
    {
      v4 = -2143088384;
      if ( v10 == 0x2000000 )
      {
        v4 = http_io_fn_12(a1, &v23);
        if ( v4 >= 0 )
        {
          v12 = v23 + 136;
LABEL_12:
          v21 = v12;
          goto LABEL_13;
        }
      }
    }
LABEL_2:
    http_io_fn_79(v20);
    return v4;
  }
  v4 = http_io_fn_31(a1, &v21, v11);
  if ( v4 < 0 )
    goto LABEL_2;
LABEL_13:
  v13 = http_io_fn_89();
  v15 = v21;
  v16 = v13;
  v17 = *(u32 *)(v21 + 152);
  v18 = v20;
  if ( v17 == 0 )
  {
LABEL_27:
    *(u32 *)(v21 + 152) = v18;
    goto LABEL_19;
  }
  for ( i = *(u32 *)(v21 + 152); ; i = v17 )
  {
    v17 = *(u32 *)(v17 + 8);
    if ( v17 == 0 )
      break;
  }
  if ( i == 0 )
  {
    v18 = v20;
    goto LABEL_27;
  }
  *(u32 *)(i + 8) = v20;
LABEL_19:
  if ( v23 != 0 )
  {
    --*(u32 *)(v23 + 32);
  }
  else if ( v22 != 0 )
  {
    --*(u32 *)(v22 + 8);
  }
  else if ( v15 != 0 )
  {
    --*(u32 *)(v15 + 8);
  }
  http_io_fn_90(v16, v15, v14);
  return 0;
}


int http_opts_fn_8(int **a1, int a2)
{
  int *v4;
  int *v5;
  int i;

  v4 = NULL;
  if ( a1 == NULL || a2 == 0 )
    return -2143088130;
  v5 = *a1;
  if ( *a1 == NULL )
    return -2143088603;
  for ( i = *v5; sceNet_lib_9CFBC7E3(i, a2) != 0; i = *v5 )
  {
    v4 = v5;
    v5 = (int *)v5[2];
    if ( v5 == NULL )
      return -2143088603;
  }
  if ( v4 != NULL )
    v4[2] = v5[2];
  else
    *a1 = (int *)v5[2];
  http_opts_fn_3(*v5);
  http_opts_fn_3(v5[1]);
  http_opts_fn_3((int)v5);
  return 0;
}


int http_opts_fn_9(int a1, int a2)
{
  int v4;
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

  v4 = -2143088639;
  v5 = 0;
  v15 = 0;
  v16 = 0;
  v17 = 0;
  if ( http_opts_fn_23() != 1 )
    return v4;
  v4 = -2143088603;
  if ( a2 == 0 )
    return v4;
  v7 = http_io_fn_43(a1);
  v4 = v7;
  if ( v7 < 0 )
    return v4;
  if ( v7 != 0 )
  {
    if ( v7 == 0x1000000 )
    {
      v4 = http_io_fn_23(a1, &v16, v8);
      if ( v4 < 0 )
        return v4;
      v9 = v16 + 176;
    }
    else
    {
      v4 = -2143088384;
      if ( v7 != 0x2000000 )
        return v4;
      v4 = http_io_fn_12(a1, &v17);
      if ( v4 < 0 )
        return v4;
      v9 = v17 + 136;
    }
    v15 = v9;
LABEL_11:
    v12 = http_io_fn_89();
    if ( v4 >= 0 )
    {
      while ( 1 )
      {
        v4 = http_opts_fn_8((int **)(v15 + 152), a2);
        if ( v4 < 0 )
          break;
        v5 = 1;
      }
    }
    if ( v17 != 0 )
    {
      --*(u32 *)(v17 + 32);
      goto LABEL_17;
    }
    v13 = v16;
    if ( v16 != 0 )
    {
      v14 = *(u32 *)(v16 + 8);
    }
    else
    {
      v13 = v15;
      if ( v15 == 0 )
        goto LABEL_17;
      v14 = *(u32 *)(v15 + 8);
    }
    *(u32 *)(v13 + 8) = v14 - 1;
LABEL_17:
    http_io_fn_90(v12, v10, v11);
    if ( v5 != 0 )
      return 0;
    return v4;
  }
  result = http_io_fn_31(a1, &v15, v8);
  v4 = result;
  if ( result >= 0 )
    goto LABEL_11;
  return result;
}


int sceHttpSetProxy(int a1, int a2, int a3, int a4, int a5)
{
  int v10; // dc
  int result;
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
  int v27;
  int v28;
  int v29;
  int v30;
  u32 v31[3];

  v10 = _sceKernelCheckThreadStack() < 10192;
  result = -2143088522;
  if ( !v10 )
  {
    v30 = 0;
    v12 = 0;
    v31[0] = 0;
    v13 = -2143088639;
    if ( http_opts_fn_23() == 1 )
    {
      if ( a3 == 1 )
      {
        v13 = -2143088130;
        if ( a4 == 0 )
          goto LABEL_3;
        v13 = -2143088380;
        if ( (unsigned int)(sceNet_lib_D8722983(a4) + 1) >= 0x81 )
          goto LABEL_3;
        v28 = sceNet_lib_D8722983(a4);
        v29 = http_opts_fn_2(v28 + 1);
        v12 = v29;
        v13 = -2143088606;
        if ( v29 == 0 )
          goto LABEL_3;
        sceNet_lib_80C9F02A(v29, a4);
        http_io_fn_35(v12);
      }
      v19 = http_io_fn_43(a1);
      v13 = v19;
      if ( v19 >= 0 )
      {
        if ( v19 != 0 )
        {
          v13 = -2143088384;
          if ( v19 != 0x1000000 )
            goto LABEL_3;
          v13 = http_io_fn_23(a1, v31, v20);
          if ( v13 < 0 )
            goto LABEL_3;
          v30 = v31[0] + 176;
          *(u32 *)(v31[0] + 64) = 0;
        }
        else
        {
          v13 = http_io_fn_31(a1, &v30, v20);
          if ( v13 < 0 )
          {
            v14 = v31[0];
LABEL_4:
            if ( v14 != 0 )
            {
              v15 = http_io_fn_89();
              v18 = v31[0];
            }
            else
            {
              if ( v30 == 0 )
                goto LABEL_6;
              v15 = http_io_fn_89();
              v18 = v30;
            }
            --*(u32 *)(v18 + 8);
            http_io_fn_90(v15, v16, v17);
LABEL_6:
            result = v13;
            if ( v12 != 0 )
            {
              http_opts_fn_3(v12);
              return v13;
            }
            return result;
          }
        }
        v13 = http_io_fn_4(v30);
        if ( v13 >= 0 )
        {
          v23 = http_io_fn_89();
          if ( a2 != 0 )
          {
            if ( a3 == 0 )
            {
              *(u16 *)(v30 + 96) = 1;
              goto LABEL_22;
            }
            v25 = v31[0];
            if ( a3 != 1 )
            {
LABEL_23:
              if ( v25 != 0 )
              {
                v26 = *(u32 *)(v25 + 8);
              }
              else
              {
                v25 = v30;
                if ( v30 == 0 )
                {
LABEL_28:
                  http_io_fn_90(v23, v21, v22);
                  return 0;
                }
                v26 = *(u32 *)(v30 + 8);
              }
              *(u32 *)(v25 + 8) = v26 - 1;
              goto LABEL_28;
            }
            v24 = v30;
            v27 = *(u32 *)(v30 + 64);
            if ( v27 != 0 )
            {
              http_opts_fn_3(v27);
              v24 = v30;
            }
            *(u32 *)(v24 + 64) = v12;
            *(u16 *)(v24 + 172) = a5;
            *(u32 *)(v24 + 16) = 1;
          }
          else
          {
            v24 = v30;
            *(u32 *)(v30 + 16) = 0;
          }
          *(u16 *)(v24 + 96) = 0;
LABEL_22:
          v25 = v31[0];
          goto LABEL_23;
        }
      }
    }
LABEL_3:
    v14 = v31[0];
    goto LABEL_4;
  }
  return result;
}


int sceHttpGetProxy(int a1, u32 *a2, u32 *a3, int a4, unsigned int a5, u16 *a6)
{
  int v12; // dc
  int result;
  int v14;
  s64 v15;
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

  v12 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v12 )
  {
    v27 = 0;
    v14 = -2143088639;
    v28 = 0;
    v29 = 0;
    if ( http_opts_fn_23() != 1 )
      return v14;
    v15 = ((u64)(v15) & 0xFFFFFFFF00000000ULL) | (u32)(a3 == NULL);
    v15 = ((u64)(v15) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(a2 == NULL) << 32);
    if ( v15 != 0 || a4 == 0 || a6 == NULL )
      return -2143088130;
    v14 = -2143088380;
    if ( a5 < 0x80 )
      return v14;
    v16 = http_io_fn_43(a1);
    v14 = v16;
    if ( v16 < 0 )
      return v14;
    if ( v16 != 0 )
    {
      if ( v16 == 0x1000000 )
      {
        v14 = http_io_fn_23(a1, &v28, v17);
        if ( v14 < 0 )
          return v14;
        v19 = v28;
      }
      else
      {
        if ( v16 != 0x2000000 )
          return -2143088384;
        v18 = http_io_fn_12(a1, &v29);
        v14 = v18;
        if ( v18 < 0 )
          return v14;
        v19 = *(u32 *)(v29 + 928);
      }
      v27 = v19 + 176;
    }
    else
    {
      result = http_io_fn_31(a1, &v27, v17);
      v14 = result;
      if ( result < 0 )
        return result;
    }
    v20 = http_io_fn_89();
    v22 = v27;
    v23 = v20;
    if ( *(u32 *)(v27 + 16) == 1 )
    {
      v24 = *(u32 *)(v27 + 64);
      *a2 = 1;
      if ( v24 != 0 )
      {
        sceNet_lib_B5CE388A(a4, v24, 128);
        v22 = v27;
        *a6 = *(u16 *)(v27 + 172);
      }
    }
    else
    {
      *a2 = 0;
    }
    v25 = *(s16 *)(v22 + 96);
    v26 = v29;
    v12 = v29 == 0;
    *a3 = v25 != 1;
    if ( v12 )
    {
      if ( v28 != 0 )
      {
        v21 = *(u32 *)(v28 + 8) - 1;
        *(u32 *)(v28 + 8) = v21;
      }
      else if ( v22 != 0 )
      {
        --*(u32 *)(v22 + 8);
      }
    }
    else
    {
      --*(u32 *)(v26 + 32);
    }
    http_io_fn_90(v23, v25, v21);
    return v14;
  }
  return result;
}


int sceHttpEnableCache(int a1)
{
  int v2; // dc
  int result;

  v2 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v2 )
  {
    if ( http_opts_fn_23() == 1 && http_io_fn_66() == 1 )
      return http_opts_fn_10(a1, 1);
    else
      return -2143088639;
  }
  return result;
}


int sceHttp_8046E250(int a1)
{
  int v2; // dc
  int result;

  v2 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v2 )
  {
    if ( http_opts_fn_23() == 1 && http_io_fn_68() == 1 )
      return http_opts_fn_10(a1, 2);
    else
      return -2143088639;
  }
  return result;
}


int sceHttpAddExtraHeader(int a1, int a2, int a3, int a4)
{
  int v8; // dc
  int result;

  v8 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v8 )
    return http_opts_fn_7(a1, a2, a3, a4);
  return result;
}


int sceHttpDeleteHeader(int a1, int a2)
{
  int v4; // dc
  int result;

  v4 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v4 )
    return http_opts_fn_9(a1, a2);
  return result;
}


int http_opts_fn_10(int a1, int a2)
{
  int v4;
  int v5;
  int v6;
  int v7;
  int v9;
  u32 *v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  u32 *v18;
  u32 *v19;
  int v20;

  v18 = NULL;
  v19 = NULL;
  v4 = http_io_fn_43(a1);
  if ( v4 >= 0 )
  {
    v7 = http_io_fn_89();
    switch ( v4 )
    {
      case 0:
        v9 = http_io_fn_31(a1, &v18, v6);
        v10 = v18;
        v4 = v9;
        if ( v18 == NULL )
          goto LABEL_6;
        v11 = v18[12];
        v18[5] = a2;
        if ( v11 != 0 )
          goto LABEL_12;
        v5 = 1;
        v6 = 2;
        if ( a2 == 1 )
        {
          v10[12] = http_io_fn_67();
        }
        else
        {
          if ( a2 != 2 )
            goto LABEL_12;
          v10[12] = http_io_fn_69();
        }
        v10 = v18;
        goto LABEL_12;
      case 0x1000000:
        v16 = http_io_fn_23(a1, &v19, v6);
        v10 = v19;
        v4 = v16;
        if ( v19 == NULL )
          goto LABEL_6;
        v17 = v19[56];
        v19[49] = a2;
        if ( v17 == 0 )
        {
          if ( a2 == 1 )
          {
            v10[56] = http_io_fn_67();
          }
          else
          {
            if ( a2 != 2 )
            {
              v12 = v10[2];
              goto LABEL_13;
            }
            v10[56] = http_io_fn_69();
          }
          v10 = v19;
        }
LABEL_12:
        v12 = v10[2];
LABEL_13:
        v4 = 0;
        v10[2] = v12 - 1;
        goto LABEL_6;
      case 0x2000000:
        v13 = http_io_fn_12(a1, &v20);
        v14 = v20;
        v4 = v13;
        if ( v20 == 0 )
          goto LABEL_6;
        v15 = *(u32 *)(v20 + 184);
        *(u32 *)(v20 + 156) = a2;
        if ( v15 == 0 )
        {
          if ( a2 == 1 )
          {
            *(u32 *)(v14 + 184) = http_io_fn_67();
          }
          else
          {
            if ( a2 != 2 )
              goto LABEL_22;
            *(u32 *)(v14 + 184) = http_io_fn_69();
          }
          v14 = v20;
        }
LABEL_22:
        --*(u32 *)(v14 + 32);
        goto LABEL_6;
      default:
        v4 = -2143088384;
LABEL_6:
        http_io_fn_90(v7, v5, v6);
        return v4;
    }
  }
  return v4;
}


int http_opts_fn_11()
{
  int v0;
  unsigned int v1;
  int v2;

  v0 = 0;
  if ( (&HTTP_BSS_U32(0x2680))[0] == 1 )
    return -2143088608;
  (&HTTP_BSS_U32(0x2680))[0] = 1;
  sceKernelMemset(&HTTP_BSS_U8(0x2684), 0, 1088);
  v1 = 0;
  HTTP_BSS_U32(0x2AC4) = (int)&HTTP_BSS_U8(0x2684);
  v2 = 0;
  do
  {
    ++v1;
    (&HTTP_BSS_U32(0x2680))[v2 + 17] = (int)&HTTP_BSS_U8(0x26C8) + v2 * 4;
    v2 += 17;
  }
  while ( v1 < 0xF );
  HTTP_BSS_U32(0x2ACC) = 0;
  HTTP_BSS_U32(0x2AC0) = 0;
  HTTP_BSS_U32(0x2AC8) = 0;
  return v0;
}


int http_opts_fn_12()
{
  int v0;
  int *i;

  v0 = 0;
  if ( (&HTTP_BSS_U32(0x2680))[0] != 1 )
    return -2143088639;
  for ( i = (int *)HTTP_BSS_U32(0x2AC8); i != NULL; i = (int *)i[15] )
  {
    http_opts_fn_3(i[12]);
    http_opts_fn_3(i[6]);
  }
  (&HTTP_BSS_U32(0x2680))[0] = 0;
  sceKernelMemset(&HTTP_BSS_U8(0x2684), 0, 1088);
  HTTP_BSS_U32(0x2ACC) = 0;
  HTTP_BSS_U32(0x2AC4) = 0;
  HTTP_BSS_U32(0x2AC8) = 0;
  return v0;
}


int http_opts_fn_13(int a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;

  v2 = 0;
  v3 = http_io_fn_14(a1);
  if ( v3 != 0 )
  {
    v4 = http_io_fn_89();
    v2 = http_opts_fn_22(v3, *(u32 *)(a1 + 28), 0);
    http_io_fn_90(v4, v5, v6);
    http_opts_fn_3(v3);
  }
  return v2;
}


int http_opts_fn_14(int a1, int a2, int a3, int a4)
{
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int *v13;
  int v14;
  int i;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;

  v11 = http_io_fn_89();
  v8 = http_opts_fn_22(a1, a2, a3);
  v12 = v8;
  if ( v8 == 0 )
  {
LABEL_4:
    while ( 1 )
    {
      v13 = (int *)HTTP_BSS_U32(0x2AC4);
      if ( HTTP_BSS_U32(0x2AC4) != 0 )
        break;
      v14 = HTTP_BSS_U32(0x2ACC);
      if ( HTTP_BSS_U32(0x2ACC) == 0 )
        return http_io_fn_90(v11, v9, v10);
      for ( i = *(u32 *)(HTTP_BSS_U32(0x2ACC) + 4); i != 0; i = *(u32 *)(v14 + 4) )
      {
        v14 = *(u32 *)(v14 + 56);
        if ( v14 == 0 )
          return http_io_fn_90(v11, v9, v10);
      }
      http_opts_fn_20(v14);
    }
    HTTP_BSS_U32(0x2AC4) = *(u32 *)(HTTP_BSS_U32(0x2AC4) + 64);
    v17 = sceNet_lib_D8722983(a1);
    v18 = http_opts_fn_2(v17 + 1);
    v13[6] = v18;
    if ( v18 != 0 )
    {
      sceNet_lib_80C9F02A(v18, a1);
      v19 = sceNet_lib_D8722983(a4);
      v20 = http_opts_fn_2(v19 + 1);
      v13[12] = v20;
      if ( v20 != 0 )
      {
        sceNet_lib_80C9F02A(v20, a4);
        v21 = sceNet_lib_D8722983(a1);
        http_io_fn_91(a1, v21, v13 + 2);
        v13[7] = a2;
        if ( a3 != 0 )
          sceKernelMemcpy(v13 + 8, a3, 16);
        v13[14] = 0;
        v22 = HTTP_BSS_U32(0x2AC8);
        v13[15] = HTTP_BSS_U32(0x2AC8);
        if ( v22 != 0 )
          *(u32 *)(v22 + 56) = v13;
        else
          HTTP_BSS_U32(0x2ACC) = (int)v13;
        HTTP_BSS_U32(0x2AC8) = (int)v13;
        return http_io_fn_90(v11, v9, v10);
      }
      http_opts_fn_3(v13[6]);
    }
    sceKernelMemset(v13, 0, 68);
    v9 = HTTP_BSS_U32(0x2AC4);
    v13[16] = HTTP_BSS_U32(0x2AC4);
    HTTP_BSS_U32(0x2AC4) = (int)v13;
    return http_io_fn_90(v11, v9, v10);
  }
  if ( sceNet_lib_A0F16ABD(*(u32 *)(v8 + 48), a4) != 0 )
  {
    http_opts_fn_20(v12);
    http_opts_fn_18(v12);
    goto LABEL_4;
  }
  http_opts_fn_18(v12);
  return http_io_fn_90(v11, v9, v10);
}


int http_opts_fn_15(int a1, int a2, int a3)
{
  int v6;
  int v7;
  int v8;
  int v9;

  v6 = http_io_fn_89();
  v7 = http_opts_fn_22(a1, a2, a3);
  http_io_fn_90(v6, v8, v9);
  return v7;
}


int http_opts_fn_16(int a1)
{
  int v2;
  int v3;
  int v4;

  v2 = http_io_fn_89();
  http_opts_fn_18(a1);
  return http_io_fn_90(v2, v3, v4);
}


int http_opts_fn_17(int a1)
{
  int v2;
  int v3;
  int v4;

  v2 = http_io_fn_89();
  http_opts_fn_20(a1);
  http_opts_fn_18(a1);
  return http_io_fn_90(v2, v3, v4);
}


void http_opts_fn_18(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;

  v2 = a1[1];
  a1[1] = v2 - 1;
  if ( v2 == 1 && a1[13] == 1 )
  {
    v3 = a1[14];
    if ( v3 != 0 )
    {
      *(u32 *)(v3 + 60) = a1[15];
      v5 = a1[15];
      if ( v5 != 0 )
        *(u32 *)(v5 + 56) = v3;
      else
        HTTP_BSS_U32(0x2ACC) = v3;
    }
    else
    {
      HTTP_BSS_U32(0x2AC8) = a1[15];
      v4 = a1[15];
      if ( v4 != 0 )
        *(u32 *)(v4 + 56) = 0;
      else
        HTTP_BSS_U32(0x2ACC) = 0;
    }
    http_opts_fn_3(a1[6]);
    http_opts_fn_3(a1[12]);
    sceKernelMemset(a1, 0, 68);
    a1[16] = HTTP_BSS_U32(0x2AC4);
    HTTP_BSS_U32(0x2AC4) = (int)a1;
  }
}


void http_opts_fn_19(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;

  v2 = a1[1];
  a1[1] = v2 - 1;
  if ( v2 == 1 && a1[13] == 1 )
  {
    http_io_fn_71(&HTTP_BSS_U8(0x30DC), a1);
    v3 = a1[14];
    if ( v3 != 0 )
    {
      *(u32 *)(v3 + 60) = a1[15];
      v5 = a1[15];
      if ( v5 != 0 )
        *(u32 *)(v5 + 56) = v3;
      else
        HTTP_BSS_U32(0x316C) = v3;
    }
    else
    {
      HTTP_BSS_U32(0x3168) = a1[15];
      v4 = a1[15];
      if ( v4 != 0 )
        *(u32 *)(v4 + 56) = 0;
      else
        HTTP_BSS_U32(0x316C) = 0;
    }
    http_io_fn_56(a1[12]);
    http_io_fn_55(a1[12]);
    http_opts_fn_3(a1[6]);
    http_opts_fn_3((int)a1);
  }
}


void http_opts_fn_20(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;

  v2 = a1[1];
  a1[13] = 1;
  if ( v2 == 0 )
  {
    v3 = a1[14];
    if ( v3 != 0 )
    {
      *(u32 *)(v3 + 60) = a1[15];
      v5 = a1[15];
      if ( v5 != 0 )
        *(u32 *)(v5 + 56) = v3;
      else
        HTTP_BSS_U32(0x2ACC) = v3;
    }
    else
    {
      HTTP_BSS_U32(0x2AC8) = a1[15];
      v4 = a1[15];
      if ( v4 != 0 )
        *(u32 *)(v4 + 56) = 0;
      else
        HTTP_BSS_U32(0x2ACC) = 0;
    }
    http_opts_fn_3(a1[6]);
    http_opts_fn_3(a1[12]);
    sceKernelMemset(a1, 0, 68);
    a1[16] = HTTP_BSS_U32(0x2AC4);
    HTTP_BSS_U32(0x2AC4) = (int)a1;
  }
}


void http_opts_fn_21(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;

  v2 = a1[1];
  a1[13] = 1;
  if ( v2 == 0 )
  {
    http_io_fn_71(&HTTP_BSS_U8(0x30DC), a1);
    v3 = a1[14];
    if ( v3 != 0 )
    {
      *(u32 *)(v3 + 60) = a1[15];
      v5 = a1[15];
      if ( v5 != 0 )
        *(u32 *)(v5 + 56) = v3;
      else
        HTTP_BSS_U32(0x316C) = v3;
    }
    else
    {
      HTTP_BSS_U32(0x3168) = a1[15];
      v4 = a1[15];
      if ( v4 != 0 )
        *(u32 *)(v4 + 56) = 0;
      else
        HTTP_BSS_U32(0x316C) = 0;
    }
    http_io_fn_56(a1[12]);
    http_io_fn_55(a1[12]);
    http_opts_fn_3(a1[6]);
    http_opts_fn_3((int)a1);
  }
}


int *http_opts_fn_22(int a1, int a2, int a3)
{
  int v6;
  int *v7;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  u8 v14[16];

  v6 = sceNet_lib_D8722983(a1);
  http_io_fn_91(a1, v6, v14);
  v7 = (int *)HTTP_BSS_U32(0x2AC8);
  if ( HTTP_BSS_U32(0x2AC8) == 0 )
    return v7;
  while ( sceNet_lib_E0A81C7C(v7 + 2, v14, 16) != 0 )
  {
    v7 = (int *)v7[15];
LABEL_4:
    if ( v7 == NULL )
      return v7;
  }
  if ( sceNet_lib_A0F16ABD(v7[6], a1) != 0 )
  {
    v7 = (int *)v7[15];
    goto LABEL_4;
  }
  if ( v7[7] != a2 )
  {
    v7 = (int *)v7[15];
    goto LABEL_4;
  }
  if ( a3 != 0 && sceNet_lib_E0A81C7C(v7 + 8, a3, 16) != 0 )
  {
    v7 = (int *)v7[15];
    goto LABEL_4;
  }
  if ( v7[13] == 1 )
  {
    v7 = (int *)v7[15];
    goto LABEL_4;
  }
  v9 = *v7 + 1;
  ++v7[1];
  *v7 = v9;
  if ( v7 != NULL )
  {
    v10 = v7[14];
    if ( v10 != 0 )
    {
      *(u32 *)(v10 + 60) = v7[15];
      v13 = v7[15];
      if ( v13 != 0 )
        *(u32 *)(v13 + 56) = v10;
      else
        HTTP_BSS_U32(0x2ACC) = v10;
    }
    else
    {
      HTTP_BSS_U32(0x2AC8) = v7[15];
      v11 = v7[15];
      if ( v11 != 0 )
        *(u32 *)(v11 + 56) = 0;
      else
        HTTP_BSS_U32(0x2ACC) = 0;
    }
    v7[14] = 0;
    v12 = HTTP_BSS_U32(0x2AC8);
    v7[15] = HTTP_BSS_U32(0x2AC8);
    if ( v12 != 0 )
      *(u32 *)(v12 + 56) = v7;
    else
      HTTP_BSS_U32(0x2ACC) = (int)v7;
    HTTP_BSS_U32(0x2AC8) = (int)v7;
  }
  return v7;
}


int sceHttpInit(int a1)
{
  int v2; // dc
  int result;
  int v4;

  v2 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v2 )
  {
    v4 = -2143088608;
    if ( HTTP_BSS_U32(0x2AD0) == 1 )
      return v4;
    http_io_fn_34();
    http_io_fn_25();
    http_io_fn_15();
    http_opts_fn_11();
    http_core_fn_18();
    http_cookie_fn_14();
    v4 = http_opts_fn_4(a1);
    result = 0;
    if ( v4 < 0 )
      return v4;
    else
      HTTP_BSS_U32(0x2AD0) = 1;
  }
  return result;
}


int sceHttpEnd()
{
  int v0; // dc
  int result;
  int v2;

  v0 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v0 )
  {
    v2 = -2143088639;
    if ( HTTP_BSS_U32(0x2AD0) == 1 )
    {
      http_cookie_fn_15();
      http_opts_fn_12();
      http_core_fn_19();
      http_opts_fn_5();
      HTTP_BSS_U32(0x2AD0) = 0;
      return 0;
    }
    return v2;
  }
  return result;
}


int http_opts_fn_23()
{
  return HTTP_BSS_U32(0x2AD0);
}


int module_reboot_before()
{
  while ( http_io_fn_104() < 0 )
    sceKernelDelayThread(100000);
  return 0;
}


int http_opts_fn_24(u32 *a1, int a2, unsigned int a3, int a4)
{
  int result;
  unsigned int v8;
  int v9;
  int v10;
  unsigned int i;
  int v12;
  unsigned int v13;
  int v14;

  result = 0;
  v8 = a3;
  v9 = 0;
  if ( a1[15] != 4 )
    return result;
  v10 = a1[233];
  if ( v10 == 0 )
    v10 = a1[232];
  if ( a3 == 0 )
    return 0;
  for ( i = a1[22]; i != 0; i = a1[22] )
  {
    v12 = v8 < i;
LABEL_10:
    v13 = v8;
    if ( !v12 )
      v13 = i;
    result = http_opts_fn_25(a1, a2 + v9, v13, a4);
    v9 += result;
    if ( result < 0
      || (v14 = a1[22], v8 -= result, a1[22] = v14 - result, v14 == result) && (result = http_io_fn_36(a1, a4)) < 0 )
    {
      a1[22] = 0;
      goto LABEL_20;
    }
LABEL_14:
    result = v9;
    if ( v8 == 0 || a1[15] != 4 )
      return result;
  }
  result = http_opts_fn_26(a1, a1 + 22, a4);
  if ( result < 0 )
  {
    a1[22] = 0;
    goto LABEL_20;
  }
  i = a1[22];
  v12 = v8 < i;
  if ( i != 0 )
    goto LABEL_10;
  result = http_io_fn_3(a1, a4);
  if ( result >= 0 )
  {
    if ( a1[212] == 1 )
      http_io_fn_24(v10);
    a1[211] = 0;
    a1[15] = 5;
    goto LABEL_14;
  }
  a1[22] = 0;
LABEL_20:
  a1[211] = 0;
  return result;
}


int http_opts_fn_25(int a1, int a2, int a3, int a4)
{
  return http_io_fn_7(a1, a2, a3, a4, 0);
}


int http_opts_fn_26(int a1, u32 *a2, int a3)
{
  int v3;
  int v6;
  int v8;
  char *v9;
  int v10;
  int v11;
  int v12;
  s64 v13;
  u8 *v14;
  int result;
  int v16;
  int v17;
  u8 v18[16];
  u8 v19[16];

  v3 = 0;
  v6 = *(u32 *)(a1 + 932);
  if ( v6 == 0 )
    v6 = *(u32 *)(a1 + 928);
  v8 = 0;
  v9 = v18;
  while ( 1 )
  {
    v10 = http_opts_fn_25(a1, (int)v9, 1, a3);
    v11 = v10;
    if ( v10 < 0 )
      return v11;
    if ( v10 == 0 )
      goto LABEL_19;
    v12 = *v9;
    v13 = ((u64)(v13) & 0xFFFFFFFF00000000ULL) | (u32)(v12 == 0x20);
    v13 = ((u64)(v13) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v12 == 0xD) << 32);
    if ( v13 != 0 )
      break;
    v9 = &v18[++v8];
    if ( v8 >= 11 )
      goto LABEL_9;
  }
  v3 = 1;
LABEL_9:
  v14 = &v18[v8];
  if ( v3 != 1 )
    return 0;
  v16 = (char)*v14;
  if ( v16 == 32 )
  {
    result = http_io_fn_36(a1, a3);
    if ( result >= 0 )
      goto LABEL_13;
  }
  else
  {
    if ( v16 != 13 )
    {
LABEL_13:
      *v14 = 0;
LABEL_14:
      *a2 = sceNet_lib_2A73ADDC(v18, 0, 16);
      return 0;
    }
    v17 = http_opts_fn_25(a1, (int)v19, 1, a3);
    v11 = v17;
    if ( v17 >= 0 )
    {
      if ( v17 != 0 && v19[0] == 10 )
      {
        *v14 = 0;
        goto LABEL_14;
      }
LABEL_19:
      v11 = -2143088540;
      http_io_fn_24(v6);
    }
    return v11;
  }
  return result;
}


int http_opts_fn_27(u32 *a1, int a2, int a3)
{
  if ( a1[1] != 1 || a1[13] == 3 )
    return sceNetInetRecv(a1[12]);
  else
    return http_ssl_fn_14(a1, a2, a3);
}


int http_opts_fn_28(int a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  u8 *v7;
  int started;
  int v9;
  int v10;
  u32 v11;
  int result;
  u8 v13[128];
  u32 v14[4];

  v2 = 0;
  v3 = *(s16 *)(a1 + 272);
  v14[0] = 0;
  if ( v3 != 1 || (result = http_io_fn_4(a1 + 176)) >= 0 )
  {
    v6 = http_io_fn_89();
    if ( *(u32 *)(a1 + 192) == 1 )
    {
      sceNet_lib_B5CE388A(v13, *(u32 *)(a1 + 240), 128);
      v7 = v13;
    }
    else
    {
      v7 = *(u8 **)(a1 + 56);
    }
    http_io_fn_90(v6, v4, v5);
    started = http_io_fn_95(2, v7, a1 + 356);
    if ( started == 1 )
      goto LABEL_9;
    v2 = http_opts_fn_2(768);
    started = -2143088606;
    if ( v2 != 0 )
    {
      started = sceNetResolverCreate(v14, v2, 768);
      if ( started >= 0 )
      {
        v9 = v14[0];
        *(u32 *)(a1 + 52) = 1;
        *(u32 *)(a1 + 28) = v9;
        started = sceNetResolverStartNtoA(v9, v7, a1 + 356, *(u32 *)(a1 + 200), *(u32 *)(a1 + 188));
        *(u32 *)(a1 + 52) = 2;
        if ( started < 0 )
        {
          sceNetResolverDelete(v14[0]);
        }
        else
        {
          started = sceNetResolverDelete(v14[0]);
          if ( started >= 0 )
          {
LABEL_9:
            v10 = *(u32 *)(a1 + 192);
            *(u8 *)(a1 + 353) = 2;
            *(u32 *)(a1 + 64) = 1;
            if ( v10 == 1 )
              v11 = *(u16 *)(a1 + 348);
            else
              v11 = *(u16 *)(a1 + 400);
            *(u16 *)(a1 + 354) = _wsbh(v11);
          }
        }
      }
    }
    result = started;
    if ( v2 != 0 )
    {
      http_opts_fn_3(v2);
      return started;
    }
  }
  return result;
}


