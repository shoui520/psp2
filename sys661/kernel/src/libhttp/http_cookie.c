#include "libhttp_internal.h"

int *http_cookie_fn_1(u32 *a1, unsigned int a2, int a3)
{
  int v6;
  int *v7;
  int v8;
  int v9;
  int v10;
  int i;
  int v12;
  int v13;
  int v14;
  int v15;
  int v17;
  int v18;
  unsigned int v19;
  int v20;
  int v21;
  u8 v22[16];
  int v23;
  int v24;

  v6 = a1[233];
  v7 = NULL;
  if ( v6 == 0 )
    v6 = a1[232];
  v23 = http_io_fn_89();
  v10 = HTTP_BSS_U32(0x2648);
  if ( HTTP_BSS_U32(0x2648) != 0 )
  {
    v24 = a2 < 2;
    for ( i = *(u32 *)(HTTP_BSS_U32(0x2648) + 564); ; i = *(u32 *)(v10 + 564) )
    {
      if ( i == 1 )
        goto LABEL_6;
      sceKernelMemset(v22, 0, 16);
      v18 = v10 + 32;
      if ( a3 != 0 )
      {
        if ( sceNet_lib_D8722983(a3) != 0 )
        {
          v21 = sceNet_lib_D8722983(a3);
          http_io_fn_91(a3, v21, v22);
        }
        v18 = v10 + 32;
      }
      if ( sceNet_lib_E0A81C7C(v18, v22, 16) == 0 )
      {
        v8 = *(u32 *)(v10 + 12);
        if ( a2 == v8 )
        {
          v9 = v24;
          if ( v24 )
          {
            if ( *(u32 *)(v6 + 4) == *(u32 *)(v10 + 16) )
            {
              if ( *(u16 *)(v6 + 400) == *(u16 *)(v10 + 20) )
              {
                if ( sceNet_lib_A0F16ABD(*(u32 *)(v6 + 56), *(u32 *)(v10 + 24)) != 0 )
                {
                  v10 = *(u32 *)(v10 + 604);
                }
                else if ( http_cookie_fn_10(*(u32 *)(v10 + 28), a1[28]) == 1 )
                {
                  if ( v7 == NULL )
                  {
                    v7 = (int *)v10;
LABEL_6:
                    v10 = *(u32 *)(v10 + 604);
                    goto LABEL_7;
                  }
                  v19 = sceNet_lib_D8722983(*(u32 *)(v10 + 28));
                  if ( v19 >= sceNet_lib_D8722983(v7[7]) )
                  {
                    v7 = (int *)v10;
                    goto LABEL_6;
                  }
                  v10 = *(u32 *)(v10 + 604);
                }
                else
                {
                  v10 = *(u32 *)(v10 + 604);
                }
              }
              else
              {
                v10 = *(u32 *)(v10 + 604);
              }
            }
            else
            {
              v10 = *(u32 *)(v10 + 604);
            }
          }
          else if ( *(u16 *)(v6 + 348) == *(u16 *)(v10 + 20) )
          {
            v20 = *(u32 *)(v6 + 240);
            if ( v20 != 0 )
            {
              if ( sceNet_lib_A0F16ABD(v20, *(u32 *)(v10 + 24)) == 0 )
              {
                v7 = (int *)v10;
                goto LABEL_6;
              }
              v10 = *(u32 *)(v10 + 604);
            }
            else
            {
              v10 = *(u32 *)(v10 + 604);
            }
          }
          else
          {
            v10 = *(u32 *)(v10 + 604);
          }
        }
        else
        {
          v10 = *(u32 *)(v10 + 604);
        }
      }
      else
      {
        v10 = *(u32 *)(v10 + 604);
      }
LABEL_7:
      if ( v10 == 0 )
      {
        if ( v7 != NULL )
        {
          v12 = v7[150];
          v13 = *v7 + 1;
          ++v7[1];
          *v7 = v13;
          if ( v12 != 0 )
          {
            *(u32 *)(v12 + 604) = v7[151];
            v17 = v7[151];
            if ( v17 != 0 )
              *(u32 *)(v17 + 600) = v12;
            else
              HTTP_BSS_U32(0x264C) = v12;
          }
          else
          {
            HTTP_BSS_U32(0x2648) = v7[151];
            v14 = v7[151];
            if ( v14 != 0 )
              *(u32 *)(v14 + 600) = 0;
            else
              HTTP_BSS_U32(0x264C) = 0;
          }
          v7[150] = 0;
          v15 = HTTP_BSS_U32(0x2648);
          v7[151] = HTTP_BSS_U32(0x2648);
          if ( v15 != 0 )
            *(u32 *)(v15 + 600) = v7;
          else
            HTTP_BSS_U32(0x264C) = (int)v7;
          HTTP_BSS_U32(0x2648) = (int)v7;
        }
        break;
      }
    }
  }
  http_io_fn_90(v23, v8, v9);
  return v7;
}


int http_cookie_fn_2(u32 *a1, int a2, int a3)
{
  int v5;
  int v7;
  int v8;
  int v9;
  int v10;
  int i;
  int v12;
  int v13;
  s16 v14;
  int v15;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int *v23;
  int v24;
  int v25;
  int v26;
  int v27;

  v5 = a1[233];
  if ( v5 == 0 )
    v5 = a1[232];
  v9 = http_io_fn_89();
  while ( 1 )
  {
    v10 = HTTP_BSS_U32(0x2644);
    if ( HTTP_BSS_U32(0x2644) != 0 )
      break;
    v10 = HTTP_BSS_U32(0x264C);
    if ( HTTP_BSS_U32(0x264C) == 0 )
      goto LABEL_11;
    for ( i = *(u32 *)(HTTP_BSS_U32(0x264C) + 4); i != 0; i = *(u32 *)(v10 + 4) )
    {
      v10 = *(u32 *)(v10 + 600);
      if ( v10 == 0 )
        goto LABEL_11;
    }
    http_core_fn_20((u32 *)v10);
  }
  HTTP_BSS_U32(0x2644) = *(u32 *)(HTTP_BSS_U32(0x2644) + 608);
LABEL_11:
  http_io_fn_90(v9, v7, v8);
  if ( v10 == 0 )
    return v10;
  if ( (unsigned int)(a2 - 2) >= 2 )
  {
    v20 = sceNet_lib_D8722983(*(u32 *)(v5 + 56));
    v21 = http_opts_fn_2(v20 + 1);
    *(u32 *)(v10 + 24) = v21;
    if ( v21 != 0 )
    {
      sceNet_lib_80C9F02A(v21, *(u32 *)(v5 + 56));
      v22 = a1[28];
      if ( v22 != 0 && sceNet_lib_D8722983(v22) != 0 )
        v23 = (int *)a1[28];
      else
        v23 = "/";
      v24 = sceNet_lib_F48963C6(v23, 47);
      v25 = v24 - (u32)v23;
      if ( v24 != 0 )
      {
        v27 = v25 + 1;
        v26 = http_opts_fn_2(v25 + 2);
        *(u32 *)(v10 + 28) = v26;
        if ( v26 != 0 )
        {
          sceKernelMemcpy(v26, v23, v27);
          *(u8 *)(*(u32 *)(v10 + 28) + v27) = 0;
          v14 = *(u16 *)(v5 + 400);
LABEL_15:
          *(u16 *)(v10 + 20) = v14;
          if ( a3 != 0 && sceNet_lib_D8722983(a3) != 0 )
          {
            v17 = sceNet_lib_D8722983(a3);
            http_io_fn_91(a3, v17, v10 + 32);
            v15 = *(u32 *)(v5 + 4);
          }
          else
          {
            v15 = *(u32 *)(v5 + 4);
          }
          *(u32 *)(v10 + 12) = a2;
          *(u32 *)(v10 + 16) = v15;
          *(u32 *)(v10 + 8) = 0;
          return v10;
        }
      }
    }
  }
  else
  {
    v12 = sceNet_lib_D8722983(*(u32 *)(v5 + 56));
    v13 = http_opts_fn_2(v12 + 1);
    *(u32 *)(v10 + 24) = v13;
    if ( v13 != 0 )
    {
      sceNet_lib_80C9F02A(v13, *(u32 *)(v5 + 240));
      v14 = *(u16 *)(v5 + 348);
      goto LABEL_15;
    }
  }
  v18 = *(u32 *)(v10 + 24);
  if ( v18 != 0 )
    http_opts_fn_3(v18);
  v19 = *(u32 *)(v10 + 28);
  if ( v19 != 0 )
    http_opts_fn_3(v19);
  sceKernelMemset(v10, 0, 612);
  *(u32 *)(v10 + 608) = HTTP_BSS_U32(0x2644);
  HTTP_BSS_U32(0x2644) = v10;
  return 0;
}


int http_cookie_fn_3(int a1)
{
  int v1;
  int v2;
  int v4;
  int v5;
  int v6; // dc

  v1 = http_io_fn_89();
  *(u32 *)(a1 + 600) = 0;
  v4 = v1;
  v5 = HTTP_BSS_U32(0x2648);
  v6 = HTTP_BSS_U32(0x2648) != 0;
  *(u32 *)(a1 + 604) = HTTP_BSS_U32(0x2648);
  if ( v6 )
    *(u32 *)(v5 + 600) = a1;
  else
    HTTP_BSS_U32(0x264C) = a1;
  HTTP_BSS_U32(0x2648) = a1;
  return http_io_fn_90(v4, 0x20000u, v2);
}


int sceHttp_77EE5319()
{
  int v0; // dc
  int result;
  u32 *v2;
  int v3;
  int v4;
  unsigned int v5;
  int v6;
  u16 v7;
  unsigned int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  u32 *v15;
  int v16;
  int v17;
  int v18;
  u32 *v19;
  int i;
  u8 v21[2];
  u16 v22;
  unsigned int v23;
  int v24;

  v0 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( v0 )
    return result;
  v2 = NULL;
  v3 = http_opts_fn_23();
  result = -2143088639;
  if ( v3 != 1 )
    return result;
  result = http_io_fn_104();
  if ( result < 0 )
    return result;
  if ( http_io_fn_102(0, &v23) != 1 )
    goto LABEL_5;
  v4 = http_io_fn_101(0, 1, 0);
  v5 = v23;
  if ( v4 < 0 || v23 == 0 )
    goto LABEL_5;
  do
  {
    if ( v2 == NULL )
    {
      while ( 1 )
      {
        v2 = (u32 *)HTTP_BSS_U32(0x2644);
        if ( HTTP_BSS_U32(0x2644) != 0 )
          break;
        v19 = (u32 *)HTTP_BSS_U32(0x264C);
        if ( HTTP_BSS_U32(0x264C) == 0 )
          goto LABEL_5;
        for ( i = *(u32 *)(HTTP_BSS_U32(0x264C) + 4); i != 0; i = v19[1] )
        {
          v19 = (u32 *)v19[150];
          if ( v19 == NULL )
            goto LABEL_5;
        }
        http_core_fn_20(v19);
      }
      HTTP_BSS_U32(0x2644) = *(u32 *)(HTTP_BSS_U32(0x2644) + 608);
    }
    if ( v5 < 3 )
      break;
    v6 = http_io_fn_44(0, v21, &v22);
    v7 = v22;
    if ( v6 < 0 )
      break;
    v8 = v5 - 3;
    v9 = v22;
    if ( v8 < v22 )
      break;
    if ( v21[0] == 5 )
    {
      v18 = v2[7];
      if ( v18 != 0 )
      {
        http_opts_fn_3(v18);
        v7 = v22;
      }
      v13 = http_opts_fn_2(v7);
      v2[7] = v13;
      goto LABEL_34;
    }
    if ( v21[0] >= 6u )
    {
      if ( v21[0] == 7 )
      {
        v15 = v2 + 12;
        if ( v22 != 256 )
          break;
        goto LABEL_52;
      }
      if ( v21[0] < 7u )
      {
        v15 = v2 + 8;
        if ( v22 != 16 )
          break;
        v14 = 16;
      }
      else
      {
        if ( v21[0] != 8 )
        {
          if ( v21[0] == 255 )
          {
            v2[150] = 0;
            v17 = HTTP_BSS_U32(0x2648);
            v2[2] = 1;
            v2[140] = 1;
            v2[151] = v17;
            if ( v17 != 0 )
              *(u32 *)(v17 + 600) = v2;
            else
              HTTP_BSS_U32(0x264C) = (int)v2;
            HTTP_BSS_U32(0x2648) = (int)v2;
            v2 = NULL;
            goto LABEL_19;
          }
          v9 = v22;
LABEL_18:
          http_io_fn_109(0, v22, v9, 0, 1);
          v7 = v22;
          goto LABEL_19;
        }
        v15 = v2 + 76;
        if ( v22 != 256 )
          break;
LABEL_52:
        v14 = 256;
      }
LABEL_36:
      v16 = http_io_fn_107(0, v15, v14);
      v7 = v22;
      if ( v16 < 0 )
        break;
      goto LABEL_19;
    }
    if ( v21[0] == 2 )
    {
      v15 = v2 + 4;
      if ( v22 != 4 )
        break;
      v14 = 4;
      goto LABEL_36;
    }
    if ( v21[0] >= 3u )
    {
      if ( v21[0] == 3 )
      {
        v15 = v2 + 5;
        if ( v22 != 2 )
          break;
        v14 = 2;
      }
      else
      {
        v9 = v22;
        if ( v21[0] != 4 )
          goto LABEL_18;
        v12 = v2[6];
        if ( v12 != 0 )
        {
          http_opts_fn_3(v12);
          v7 = v22;
        }
        v13 = http_opts_fn_2(v7);
        v2[6] = v13;
LABEL_34:
        v14 = v22;
        if ( v13 == 0 )
          break;
        v15 = (u32 *)v13;
      }
      goto LABEL_36;
    }
    if ( v21[0] != 1 )
      goto LABEL_18;
    if ( v22 != 4 || http_io_fn_107(0, &v24, 4) < 0 )
      break;
    v7 = v22;
    v2[3] = v24;
LABEL_19:
    v5 = v8 - v7;
  }
  while ( v5 != 0 );
  if ( v2 != NULL )
  {
    v10 = v2[6];
    if ( v10 != 0 )
      http_opts_fn_3(v10);
    v11 = v2[7];
    if ( v11 != 0 )
      http_opts_fn_3(v11);
    sceKernelMemset(v2, 0, 612);
    v2[152] = HTTP_BSS_U32(0x2644);
    HTTP_BSS_U32(0x2644) = (int)v2;
  }
LABEL_5:
  http_io_fn_106(0);
  http_io_fn_105();
  return 0;
}


int sceHttp_DD6E7857()
{
  int v0; // dc
  int result;
  int v2;
  int v3;
  int v4;
  u32 *v5;
  int i;
  s16 v7;
  int v8;
  int v9;
  s16 v10;
  int v11;
  int v12;
  u32 v13[4];

  v0 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v0 )
  {
    v2 = 1;
    v3 = http_opts_fn_23();
    v4 = 0;
    result = -2143088639;
    if ( v3 == 1 )
    {
      result = http_io_fn_104();
      if ( result >= 0 )
      {
        while ( 1 )
        {
          if ( http_io_fn_101(0, 2, 0) >= 0 )
          {
            v5 = (u32 *)HTTP_BSS_U32(0x264C);
            if ( HTTP_BSS_U32(0x264C) != 0 )
            {
              for ( i = *(u32 *)(HTTP_BSS_U32(0x264C) + 560); ; i = v5[140] )
              {
                if ( i == 1 && v5[141] != 1 )
                {
                  if ( http_io_fn_45(0, 1, 4) < 0 )
                    goto LABEL_12;
                  v13[0] = v5[3];
                  v4 += 3;
                  if ( http_io_fn_108(0, v13, 4) < 0 )
                    goto LABEL_12;
                  v4 += 4;
                  if ( http_io_fn_45(0, 2, 4) < 0 )
                    goto LABEL_12;
                  v4 += 3;
                  if ( http_io_fn_108(0, v5 + 4, 4) < 0 )
                    goto LABEL_12;
                  v4 += 4;
                  if ( http_io_fn_45(0, 3, 2) < 0 )
                    goto LABEL_12;
                  v4 += 3;
                  if ( http_io_fn_108(0, v5 + 5, 2) < 0 )
                    goto LABEL_12;
                  v4 += 2;
                  v7 = sceNet_lib_D8722983(v5[6]);
                  if ( http_io_fn_45(0, 4, (u16)(v7 + 1)) < 0 )
                    goto LABEL_12;
                  v4 += 3;
                  v8 = sceNet_lib_D8722983(v5[6]);
                  if ( http_io_fn_108(0, v5[6], v8 + 1) < 0 )
                    goto LABEL_12;
                  v9 = v4 + sceNet_lib_D8722983(v5[6]);
                  v4 = v9 + 1;
                  v10 = sceNet_lib_D8722983(v5[7]);
                  if ( http_io_fn_45(0, 5, (u16)(v10 + 1)) < 0 )
                    goto LABEL_12;
                  v4 = v9 + 4;
                  v11 = sceNet_lib_D8722983(v5[7]);
                  if ( http_io_fn_108(0, v5[7], v11 + 1) < 0 )
                    goto LABEL_12;
                  v12 = v4 + sceNet_lib_D8722983(v5[7]);
                  v4 = v12 + 1;
                  if ( http_io_fn_45(0, 6, 16) < 0 )
                    goto LABEL_12;
                  v4 = v12 + 4;
                  if ( http_io_fn_108(0, v5 + 8, 16) < 0 )
                    goto LABEL_12;
                  v4 = v12 + 20;
                  if ( http_io_fn_45(0, 7, 256) < 0 )
                    goto LABEL_12;
                  v4 = v12 + 23;
                  if ( http_io_fn_108(0, v5 + 12, 256) < 0 )
                    goto LABEL_12;
                  v4 = v12 + 279;
                  if ( http_io_fn_45(0, 8, 256) < 0 )
                    goto LABEL_12;
                  v4 = v12 + 282;
                  if ( http_io_fn_108(0, v5 + 76, 256) < 0 )
                    goto LABEL_12;
                  v4 = v12 + 538;
                  if ( http_io_fn_45(0, 255, 0) < 0 )
                    goto LABEL_12;
                  v4 = v12 + 541;
                }
                v5 = (u32 *)v5[150];
                if ( v5 == NULL )
                  break;
              }
            }
            if ( http_io_fn_106(0) >= 0 && http_io_fn_103(0, v4) >= 0 )
              break;
          }
LABEL_12:
          http_io_fn_106(0);
          if ( v2 != 0 )
          {
            v2 = 0;
            if ( http_io_fn_100(0) == 0 )
              continue;
          }
          break;
        }
        http_io_fn_105();
        return 0;
      }
    }
  }
  return result;
}


int http_cookie_fn_4(int a1)
{
  int v2;
  int result;

  v2 = *(u32 *)(a1 + 28);
  if ( v2 != 0 )
  {
    result = http_opts_fn_3(v2);
    *(u32 *)(a1 + 28) = 0;
  }
  return result;
}


int http_cookie_fn_5(int a1)
{
  int v2;
  int v3;
  int v4;

  v2 = http_io_fn_89();
  http_cookie_fn_9(a1);
  return http_io_fn_90(v2, v3, v4);
}


unsigned int http_cookie_fn_6(u32 *a1)
{
  int v2;

  http_opts_fn_3(a1[6]);
  http_opts_fn_3(a1[7]);
  v2 = a1[149];
  if ( v2 != 0 )
  {
    http_opts_fn_3(v2);
    a1[149] = 0;
  }
  sceKernelMemset(a1, 0, 612);
  a1[152] = HTTP_BSS_U32(0x2644);
  HTTP_BSS_U32(0x2644) = (int)a1;
  return 0x20000u;
}


int http_cookie_fn_7(int a1, unsigned int a2, u32 *a3, int a4, int a5, int a6)
{
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int result;

  v11 = sceNet_lib_D8722983(a4);
  v12 = v11 + sceNet_lib_D8722983("=");
  v13 = v12 + sceNet_lib_D8722983("\"");
  v14 = v13 + sceNet_lib_D8722983(a5);
  v15 = v14 + sceNet_lib_D8722983("\"");
  v16 = v15 + sceNet_lib_D8722983(",");
  v17 = v16 + sceNet_lib_D8722983(" ");
  v18 = a2 < v17 + 1;
  if ( a3 != NULL )
    *a3 = v17;
  result = 0;
  if ( a6 != 0 )
  {
    result = -2143088380;
    if ( !v18 )
    {
      sceNet_lib_5705F6F9(a1, a4);
      sceNet_lib_5705F6F9(a1, "=");
      sceNet_lib_5705F6F9(a1, "\"");
      sceNet_lib_5705F6F9(a1, a5);
      sceNet_lib_5705F6F9(a1, "\"");
      sceNet_lib_5705F6F9(a1, ",");
      sceNet_lib_5705F6F9(a1, " ");
      return 0;
    }
  }
  return result;
}


unsigned int http_cookie_fn_8(u8 *a1, int a2)
{
  unsigned int v2;
  u8 *v3;
  u8 *v4;
  unsigned int v5;
  int v6;
  unsigned int v7;
  unsigned int result;

  v2 = 0;
  v3 = a1;
  do
  {
    v4 = (u8 *)(2 * v2 + a2);
    v5 = (u8)*v3 >> 4;
    if ( v5 >= 0xA )
      *v4 = v5 + 87;
    else
      *v4 = v5 + 48;
    v6 = 2 * v2 + a2;
    v7 = *v3 & 0xF;
    result = v7 + 87;
    if ( v7 >= 0xA )
      *(u8 *)(v6 + 1) = result;
    else
      *(u8 *)(v6 + 1) = v7 + 48;
    v2 = (u16)(v2 + 1);
    v3 = &a1[v2];
  }
  while ( v2 < 0x10 );
  *(u8 *)(a2 + 32) = 0;
  return result;
}


void http_cookie_fn_9(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;

  v2 = a1[1];
  a1[1] = v2 - 1;
  if ( v2 == 1 && a1[141] == 1 )
  {
    v3 = a1[150];
    if ( v3 != 0 )
    {
      *(u32 *)(v3 + 604) = a1[151];
      v6 = a1[151];
      if ( v6 != 0 )
        *(u32 *)(v6 + 600) = v3;
      else
        HTTP_BSS_U32(0x264C) = v3;
    }
    else
    {
      HTTP_BSS_U32(0x2648) = a1[151];
      v4 = a1[151];
      if ( v4 != 0 )
        *(u32 *)(v4 + 600) = 0;
      else
        HTTP_BSS_U32(0x264C) = 0;
    }
    http_opts_fn_3(a1[6]);
    http_opts_fn_3(a1[7]);
    v5 = a1[149];
    if ( v5 != 0 )
    {
      http_opts_fn_3(v5);
      a1[149] = 0;
    }
    sceKernelMemset(a1, 0, 612);
    a1[152] = HTTP_BSS_U32(0x2644);
    HTTP_BSS_U32(0x2644) = (int)a1;
  }
}


int http_cookie_fn_10(int a1, int a2)
{
  int result;
  unsigned int v5;
  int v6; // dc
  int v7;

  if ( a1 == 0 || sceNet_lib_D8722983(a1) == 0 )
    return true;
  if ( a2 == 0 || sceNet_lib_D8722983(a2) == 0 )
    return false;
  v5 = sceNet_lib_D8722983(a1);
  v6 = sceNet_lib_D8722983(a2) < v5;
  result = false;
  if ( !v6 )
  {
    v7 = sceNet_lib_D8722983(a1);
    return sceNet_lib_E0A81C7C(a1, a2, v7) == 0;
  }
  return result;
}


int sceHttpSaveSystemCookie()
{
  return 0;
}


int sceHttpLoadSystemCookie()
{
  int v0; // dc
  int result;
  int v2;
  int v3;
  unsigned int v4;
  int v5;
  u16 v6;
  unsigned int v7;
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
  u32 *v18;
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
  u32 v30[2];
  unsigned int v31;
  unsigned int v32;
  char v33[2];
  u16 v34[3];
  unsigned int v35;
  unsigned int v36;
  unsigned int v37;
  int v38[3];

  v0 = _sceKernelCheckThreadStack() < 1872;
  result = -2143088522;
  if ( !v0 )
  {
    v38[0] = 0;
    sceKernelMemset(&v22, 0, 72);
    v2 = 0;
    v3 = -2143088639;
    if ( http_opts_fn_23() != 1
      || (v3 = -2143088521, (unsigned int)http_opts_fn_6(1) <= 0x207FF)
      || (HTTP_DATA_U32(0x4) = 1, (v3 = http_io_fn_96(&v35)) < 0) )
    {
LABEL_3:
      http_cookie_fn_12(&v22);
      http_io_fn_106(1);
      http_io_fn_105();
      return v3;
    }
    http_cookie_fn_15();
    http_cookie_fn_14();
    result = http_io_fn_104();
    v3 = result;
    if ( result >= 0 )
    {
      if ( http_io_fn_102(1, &v37) == 1 )
      {
        v3 = http_io_fn_101(1, 1, 0);
        if ( v3 >= 0 )
        {
          v4 = v37;
          if ( v37 != 0 )
          {
            v5 = v37 < 3;
            while ( 1 )
            {
              if ( v5 )
                goto LABEL_3;
              v3 = http_io_fn_44(1, v33, v34);
              if ( v3 < 0 )
                goto LABEL_3;
              v6 = v34[0];
              v7 = v4 - 3;
              if ( v7 < v34[0] )
                goto LABEL_3;
              if ( v33[0] == 5 )
              {
                if ( v25 != 0 )
                {
                  http_opts_fn_3(v25);
                  v6 = v34[0];
                }
                v21 = http_opts_fn_2(v6 + 1);
                v25 = v21;
                if ( v21 == 0 )
                  goto LABEL_3;
                v3 = http_io_fn_107(1, v21, v34[0]);
                if ( v3 < 0 )
                  goto LABEL_3;
                v11 = v34[0];
                v12 = v25;
                goto LABEL_40;
              }
              if ( (u8)v33[0] >= 6u )
                break;
              if ( v33[0] == 2 )
              {
                if ( v24 != 0 )
                {
                  http_opts_fn_3(v24);
                  v6 = v34[0];
                }
                v14 = http_opts_fn_2(v6 + 1);
                v24 = v14;
                if ( v14 == 0 )
                  goto LABEL_3;
                v3 = http_io_fn_107(1, v14, v34[0]);
                if ( v3 < 0 )
                  goto LABEL_3;
                v11 = v34[0];
                v12 = v24;
                goto LABEL_40;
              }
              if ( (u8)v33[0] >= 3u )
              {
                if ( v33[0] == 3 )
                {
                  if ( v22 != 0 )
                  {
                    http_opts_fn_3(v22);
                    v6 = v34[0];
                  }
                  v13 = http_opts_fn_2(v6 + 1);
                  v22 = v13;
                  if ( v13 == 0 )
                    goto LABEL_3;
                  v3 = http_io_fn_107(1, v13, v34[0]);
                  if ( v3 < 0 )
                    goto LABEL_3;
                  v11 = v34[0];
                  v12 = v22;
                  goto LABEL_40;
                }
                v8 = v34[0];
                if ( v33[0] == 4 )
                {
                  if ( v23 != 0 )
                  {
                    http_opts_fn_3(v23);
                    v6 = v34[0];
                  }
                  v10 = http_opts_fn_2(v6 + 1);
                  v23 = v10;
                  if ( v10 == 0 )
                    goto LABEL_3;
                  v3 = http_io_fn_107(1, v10, v34[0]);
                  if ( v3 < 0 )
                    goto LABEL_3;
                  v11 = v34[0];
                  v12 = v23;
LABEL_40:
                  *(u8 *)(v12 + v11) = 0;
                  goto LABEL_21;
                }
LABEL_20:
                v3 = http_io_fn_109(0, v34[0], v8, 0, 1);
LABEL_21:
                v9 = v34[0];
                goto LABEL_22;
              }
              v8 = v34[0];
              if ( v33[0] != 1 )
                goto LABEL_20;
              if ( v34[0] != 8 )
                goto LABEL_3;
              v3 = http_io_fn_107(1, &v31, 8);
              if ( v3 < 0 )
                goto LABEL_3;
              if ( v32 < v36 )
              {
                v2 = 1;
                goto LABEL_21;
              }
              v9 = v34[0];
              if ( v36 == v32 )
              {
                if ( v31 >= v35 )
                {
                  v4 = v7 - v34[0];
                  goto LABEL_23;
                }
                v2 = 1;
              }
LABEL_22:
              v4 = v7 - v9;
LABEL_23:
              v5 = v4 < 3;
              if ( v4 == 0 )
                goto LABEL_24;
            }
            if ( v33[0] == 8 )
            {
              if ( v29 != 0 )
              {
                http_opts_fn_3(v29);
                v6 = v34[0];
              }
              v20 = http_opts_fn_2(v6 + 1);
              v19 = v34[0];
              v18 = (u32 *)v20;
              v17 = 1;
              v29 = v20;
            }
            else
            {
              if ( (u8)v33[0] < 9u )
              {
                if ( v33[0] == 6 )
                {
                  if ( v26 != 0 )
                  {
                    http_opts_fn_3(v26);
                    v6 = v34[0];
                  }
                  v16 = http_opts_fn_2(v6 + 1);
                  v26 = v16;
                  if ( v16 == 0 )
                    goto LABEL_3;
                  v3 = http_io_fn_107(1, v16, v34[0]);
                  if ( v3 < 0 )
                    goto LABEL_3;
                  v11 = v34[0];
                  v12 = v26;
                  goto LABEL_40;
                }
                v8 = v34[0];
                if ( v33[0] == 7 )
                {
                  if ( v27 != 0 )
                  {
                    http_opts_fn_3(v27);
                    v6 = v34[0];
                  }
                  v15 = http_opts_fn_2(v6 + 1);
                  v27 = v15;
                  if ( v15 == 0 )
                    goto LABEL_3;
                  v3 = http_io_fn_107(1, v15, v34[0]);
                  if ( v3 < 0 )
                    goto LABEL_3;
                  v11 = v34[0];
                  v12 = v27;
                  goto LABEL_40;
                }
                goto LABEL_20;
              }
              if ( v33[0] != 9 )
              {
                v8 = v34[0];
                if ( (u8)v33[0] == 255 )
                {
                  if ( v2 != 1 && v24 != 0 && (v22 != 0 || v23 != 0) )
                  {
                    v3 = http_cookie_fn_25(v38, v22, v23, v26, v27, v24, v25, v29, v30[0], &v31, v30[1], v28, v2, 0);
                    if ( v3 < 0 )
                      goto LABEL_3;
                    v3 = http_cookie_fn_13(v38[0], *(u32 *)(v38[0] + 56), 1);
                    if ( v3 < 0 )
                      http_cookie_fn_12(v38[0]);
                  }
                  http_cookie_fn_12(&v22);
                  sceKernelMemset(&v22, 0, 72);
                  v2 = 0;
                  v38[0] = 0;
                  goto LABEL_21;
                }
                goto LABEL_20;
              }
              v17 = 0;
              if ( v34[0] != 4 )
                goto LABEL_3;
              v18 = v30;
              v19 = 4;
            }
            v3 = http_io_fn_107(v17, v18, v19);
            if ( v3 < 0 )
              goto LABEL_3;
            goto LABEL_21;
          }
LABEL_24:
          v3 = 0;
        }
      }
      goto LABEL_3;
    }
  }
  return result;
}


int sceHttpGetCookie(int a1, int a2, int a3, int a4, int a5)
{
  int v10; // dc
  int result;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  u32 v18[12];
  int v19;
  int v20;
  int v21;

  v10 = _sceKernelCheckThreadStack() < 1952;
  result = -2143088522;
  if ( v10 )
    return result;
  v20 = 0;
  v12 = a5 == 1;
  v13 = 0;
  v21 = 0;
  v14 = sceUriParse(0, a1, 0, &v19, 0);
  if ( v14 < 0 )
  {
LABEL_15:
    v17 = v20;
    goto LABEL_8;
  }
  v15 = http_opts_fn_2(v19);
  v13 = v15;
  if ( v15 != 0 )
  {
    v14 = sceUriParse(v18, a1, v15, 0, v19);
    if ( v14 >= 0 )
    {
      v16 = http_cookie_fn_18(&v20, v18[4], v18[5], v12, &v21);
      v17 = v20;
      if ( v16 < 0 || v20 == 0 )
      {
        v14 = -2143088603;
        goto LABEL_8;
      }
      v14 = http_cookie_fn_20(a2, v20, a3, a4);
    }
    goto LABEL_15;
  }
  v17 = v20;
  v14 = -2143088606;
LABEL_8:
  if ( v17 != 0 || v21 != 0 )
    http_cookie_fn_19();
  result = v14;
  if ( v13 != 0 )
  {
    http_opts_fn_3(v13);
    return v14;
  }
  return result;
}


int sceHttpAddCookie(int a1, int a2, int a3)
{
  int v6; // dc
  int result;
  int v8;
  int v9;
  u32 v10[8];
  u16 v11;
  u32 v12[4];

  v6 = _sceKernelCheckThreadStack() < 1952;
  result = -2143088522;
  if ( !v6 )
  {
    v8 = sceUriParse(0, a1, 0, v12, 0);
    if ( v8 >= 0 )
    {
      v8 = -2143088606;
      v9 = http_opts_fn_2(v12[0]);
      if ( v9 != 0 )
      {
        v8 = sceUriParse(v10, a1, v9, 0, v12[0]);
        if ( v8 >= 0 )
        {
          v8 = http_cookie_fn_22(a2, a3, v10[4], v10[5], v11, 0);
          if ( v8 >= 0 && (unsigned int)sceKernelGetCompiledSdkVersion() <= 0x3030010 )
            v8 = 0;
        }
      }
      if ( v9 != 0 )
        http_opts_fn_3(v9);
    }
    return v8;
  }
  return result;
}


int http_cookie_fn_11(u32 *a1)
{
  u32 *v1;
  int i;
  int v3;
  u32 *v4;
  u32 *v5;
  int result;

  v1 = a1;
  if ( a1 != NULL )
  {
    for ( i = *a1; ; i = *v4 )
    {
      if ( i != 0 )
      {
        http_opts_fn_3(i);
        *v1 = 0;
      }
      v3 = v1[1];
      if ( v3 != 0 )
      {
        http_opts_fn_3(v3);
        v1[1] = 0;
      }
      v4 = (u32 *)v1[2];
      v1[2] = 0;
      v5 = v1;
      v1 = v4;
      result = http_opts_fn_3(v5);
      if ( v4 == NULL )
        break;
    }
  }
  return result;
}


int http_cookie_fn_12(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;

  v2 = a1[4];
  v3 = a1[14];
  if ( v2 != 0 )
  {
    http_opts_fn_3(v2);
    a1[4] = 0;
  }
  v4 = a1[5];
  if ( v4 != 0 )
  {
    http_opts_fn_3(v4);
    a1[5] = 0;
  }
  v5 = a1[2];
  if ( v5 != 0 )
  {
    http_opts_fn_3(v5);
    a1[2] = 0;
  }
  if ( *a1 != 0 )
  {
    http_opts_fn_3(*a1);
    *a1 = 0;
  }
  v6 = a1[3];
  if ( v6 != 0 )
  {
    http_opts_fn_3(v6);
    a1[3] = 0;
  }
  v7 = a1[1];
  if ( v7 != 0 )
  {
    http_opts_fn_3(v7);
    a1[1] = 0;
  }
  v8 = a1[9];
  if ( v8 != 0 )
  {
    http_opts_fn_3(v8);
    a1[9] = 0;
  }
  return v3;
}


int http_cookie_fn_13(u32 *a1, int a2, int a3)
{
  int result;
  int v7;
  u32 *v8;
  int v9;
  u32 *v10;
  int v11;
  int v12;
  int v13;
  int i;
  int v15;
  int v16;
  u32 *v17;
  int v18;
  int v19;
  int v20;
  int v21;
  u32 *v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int j;
  u32 *v28;
  int k;
  u32 *v30;
  int v31;
  int v32;
  int v33;
  int v34;

  result = 0;
  v7 = 0;
  v8 = NULL;
  if ( a1 != NULL )
  {
    v9 = http_cookie_fn_28(a1[2]);
    v10 = (u32 *)(&HTTP_DATA_U32(0x0))[v9 + 11];
    v13 = http_io_fn_89();
    if ( v10 != NULL )
    {
      for ( i = v10[8]; sceNet_lib_A0F16ABD(i, a1[2]) != 0; i = v10[8] )
      {
        v8 = v10;
        v10 = (u32 *)*v10;
        if ( v10 == NULL )
          goto LABEL_7;
      }
      http_cookie_fn_16(v10);
      v7 = 1;
    }
LABEL_7:
    http_io_fn_90(v13, v11, v12);
    if ( v7 != 1 )
    {
      v16 = http_opts_fn_2(36);
      v17 = (u32 *)v16;
      if ( v16 == 0 )
        return -2143088606;
      sceKernelMemset(v16, 0, 36);
      v18 = sceNet_lib_D8722983(a1[2]);
      v19 = http_opts_fn_2(v18 + 1);
      v17[8] = v19;
      if ( v19 == 0 )
        return -2143088606;
      sceNet_lib_80C9F02A(v19, a1[2]);
      v17[4] = a1;
      v17[7] = a3;
      if ( v8 != NULL )
      {
        v17[1] = v8;
        *v8 = v17;
      }
      else
      {
        (&HTTP_DATA_U32(0x0))[v9 + 11] = (int)v17;
      }
      v17[3] = 0;
      v20 = HTTP_DATA_U32(0x24);
      v17[2] = HTTP_DATA_U32(0x24);
      if ( v20 != 0 )
        *(u32 *)(v20 + 12) = v17;
      else
        HTTP_DATA_U32(0x28) = (int)v17;
      HTTP_DATA_U32(0x24) = (int)v17;
      goto LABEL_16;
    }
    v15 = v10[7];
    v21 = v15 + a3 - HTTP_DATA_U32(0x18);
    if ( v21 > 0 )
    {
      v23 = v10[4];
      v24 = http_io_fn_89();
      if ( v23 != 0 )
      {
        for ( j = *(u32 *)(v23 + 32); ; j = *(u32 *)(v23 + 32) )
        {
          if ( j == 1 )
          {
            v23 = *(u32 *)(v23 + 64);
          }
          else
          {
            *(u32 *)(v23 + 32) = 1;
            --v21;
            --v10[7];
            --HTTP_DATA_U32(0x20);
            if ( v21 == 0 )
              break;
            v23 = *(u32 *)(v23 + 64);
          }
          if ( v23 == 0 )
            break;
        }
      }
      http_io_fn_90(v24, v25, v26);
      v15 = v10[7];
      v22 = (u32 *)v10[4];
    }
    else
    {
      v22 = (u32 *)v10[4];
    }
    v10[7] = v15 + a3;
    if ( v22 != NULL )
    {
LABEL_33:
      v28 = a1;
      if ( a1 == NULL )
        goto LABEL_42;
      for ( k = a1[8]; ; k = v28[8] )
      {
        if ( k == 1 )
        {
          v28 = (u32 *)v28[16];
        }
        else
        {
          if ( sceNet_lib_A0F16ABD(*v28, *v22) == 0 )
          {
            v22[8] = 1;
            --v10[7];
            --HTTP_DATA_U32(0x20);
LABEL_42:
            v30 = v22;
            v22 = (u32 *)v22[16];
            if ( v22 == NULL )
            {
              v30[16] = a1;
              goto LABEL_44;
            }
            goto LABEL_33;
          }
          v28 = (u32 *)v28[16];
        }
        if ( v28 == NULL )
          goto LABEL_42;
      }
    }
    v10[4] = a1;
LABEL_44:
    v31 = v10[3];
    if ( v31 != 0 )
    {
      *(u32 *)(v31 + 8) = v10[2];
      v34 = v10[2];
      if ( v34 == 0 )
      {
        HTTP_DATA_U32(0x28) = v31;
        v10[3] = 0;
        goto LABEL_49;
      }
      *(u32 *)(v34 + 12) = v31;
    }
    else
    {
      HTTP_DATA_U32(0x24) = v10[2];
      v32 = v10[2];
      if ( v32 != 0 )
        *(u32 *)(v32 + 12) = 0;
      else
        HTTP_DATA_U32(0x28) = 0;
    }
    v10[3] = 0;
LABEL_49:
    v33 = HTTP_DATA_U32(0x24);
    v10[2] = HTTP_DATA_U32(0x24);
    if ( v33 != 0 )
      *(u32 *)(v33 + 12) = v10;
    else
      HTTP_DATA_U32(0x28) = (int)v10;
    HTTP_DATA_U32(0x24) = (int)v10;
    http_cookie_fn_17(v10);
LABEL_16:
    result = 0;
    HTTP_DATA_U32(0xC) += a2;
    HTTP_DATA_U32(0x20) += a3;
  }
  return result;
}


int sceHttp_457D221D()
{
  int v0; // dc
  int result;
  unsigned int v2;
  int v3;
  int v4;
  int v5;
  int *v6;
  u32 *v7;
  int v8;
  u32 *v9;

  v0 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v0 )
  {
    result = -2143088639;
    if ( (&HTTP_DATA_U32(0x0))[0] != 0 )
    {
      v2 = 0;
      v5 = http_io_fn_89();
      v6 = HTTP_DATA_U32(0x0);
      do
      {
        v7 = (u32 *)v6[11];
        if ( v7 != NULL )
        {
          do
          {
            v8 = http_cookie_fn_16(v7);
            v9 = (u32 *)*v7;
            if ( v8 >= 0 )
            {
              v7[5] = 1;
              HTTP_DATA_U32(0x20) -= v7[7];
              http_cookie_fn_17(v7);
            }
            v7 = v9;
          }
          while ( v9 != NULL );
          ++v2;
        }
        else
        {
          ++v2;
        }
        ++v6;
      }
      while ( v2 < 0x10 );
      http_io_fn_90(v5, v3, v4);
      return 0;
    }
  }
  return result;
}


int *http_cookie_fn_14()
{
  int *result;

  result = HTTP_DATA_U32(0x0);
  if ( (&HTTP_DATA_U32(0x0))[0] == 0 )
  {
    (&HTTP_DATA_U32(0x0))[0] = 1;
    HTTP_DATA_U32(0x8) = 81920;
    HTTP_DATA_U32(0xC) = 0;
    HTTP_DATA_U32(0x10) = 4096;
    HTTP_DATA_U32(0x14) = 512;
    HTTP_DATA_U32(0x18) = 20;
    HTTP_DATA_U32(0x1C) = 300;
    HTTP_DATA_U32(0x24) = 0;
    HTTP_DATA_U32(0x28) = 0;
  }
  return result;
}


int http_cookie_fn_15()
{
  int result;
  unsigned int i;
  unsigned int v2;
  u32 *v3;
  u32 *v4;
  u32 *v5;
  int v6;
  int v7;
  u32 *v8;
  int v9;

  result = (&HTTP_DATA_U32(0x0))[0];
  if ( (&HTTP_DATA_U32(0x0))[0] == 1 )
  {
    for ( i = 0; i < 0x10; ++i )
    {
      v2 = i;
      v3 = (u32 *)(&HTTP_DATA_U32(0x0))[i + 11];
      if ( v3 != NULL )
      {
        do
        {
          v4 = (u32 *)v3[4];
          v5 = (u32 *)*v3;
          v6 = 0;
          while ( v4 != NULL )
          {
            v7 = http_cookie_fn_12(v4);
            v8 = v4;
            v6 += v7;
            v4 = (u32 *)v4[16];
            http_opts_fn_3(v8);
          }
          v9 = v3[8];
          HTTP_DATA_U32(0xC) -= v6;
          if ( v9 != 0 )
            http_opts_fn_3(v9);
          result = http_opts_fn_3(v3);
          v3 = v5;
        }
        while ( v5 != NULL );
      }
      (&HTTP_DATA_U32(0x0))[v2 + 11] = 0;
    }
    (&HTTP_DATA_U32(0x0))[0] = 0;
    HTTP_DATA_U32(0xC) = 0;
    HTTP_DATA_U32(0x20) = 0;
  }
  return result;
}


int http_cookie_fn_16(int a1)
{
  int result;

  if ( a1 == 0 )
    return -1;
  result = 0;
  if ( *(u32 *)(a1 + 20) == 1 )
    return -1;
  ++*(u32 *)(a1 + 24);
  return result;
}


void http_cookie_fn_17(int *a1)
{
  u32 *v1;
  int v2;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 *v10;
  u32 *v11;
  int v12;
  int v13;
  int v14;
  int *v15;
  u32 *v16;
  int v17;
  int v18;
  u32 *v19;
  int v20;
  int v21;

  v1 = NULL;
  v2 = 0;
  if ( a1 != NULL )
  {
    v4 = http_io_fn_89();
    v7 = a1[6];
    v8 = v4;
    a1[6] = v7 - 1;
    if ( v7 != 1 )
    {
LABEL_11:
      http_io_fn_90(v8, v5, v6);
      return;
    }
    v5 = 1;
    if ( a1[5] != 1 )
    {
      v10 = (u32 *)a1[4];
      if ( v10 == NULL )
        goto LABEL_13;
      do
      {
        if ( v10[8] == 1 )
        {
          v11 = (u32 *)v10[16];
          if ( v1 != NULL )
            v1[16] = v11;
          else
            a1[4] = (int)v11;
          v2 += http_cookie_fn_12(v10);
          http_opts_fn_3(v10);
        }
        else
        {
          v11 = (u32 *)v10[16];
          v1 = v10;
        }
        v10 = v11;
      }
      while ( v11 != NULL );
      if ( a1[4] == 0 )
      {
LABEL_13:
        v12 = a1[8];
        if ( v12 != 0 )
          http_opts_fn_3(v12);
        http_opts_fn_3(a1);
      }
      HTTP_DATA_U32(0xC) -= v2;
      goto LABEL_11;
    }
    v9 = a1[3];
    if ( v9 != 0 )
    {
      *(u32 *)(v9 + 8) = a1[2];
      v21 = a1[2];
      if ( v21 == 0 )
      {
        HTTP_DATA_U32(0x28) = v9;
        v14 = *a1;
        goto LABEL_25;
      }
      *(u32 *)(v21 + 12) = v9;
    }
    else
    {
      HTTP_DATA_U32(0x24) = a1[2];
      v13 = a1[2];
      if ( v13 != 0 )
        *(u32 *)(v13 + 12) = 0;
      else
        HTTP_DATA_U32(0x28) = 0;
    }
    v14 = *a1;
LABEL_25:
    if ( v14 != 0 )
      *(u32 *)(v14 + 4) = a1[1];
    v15 = (int *)a1[1];
    if ( v15 != NULL )
      *v15 = v14;
    else
      (&HTTP_DATA_U32(0x0))[http_cookie_fn_28(a1[8]) + 11] = *a1;
    v16 = (u32 *)a1[4];
    a1[1] = 0;
    v17 = 0;
    *a1 = 0;
    while ( v16 != NULL )
    {
      v18 = http_cookie_fn_12(v16);
      v19 = v16;
      v17 += v18;
      v16 = (u32 *)v16[16];
      http_opts_fn_3(v19);
    }
    v20 = a1[8];
    HTTP_DATA_U32(0xC) -= v17;
    if ( v20 != 0 )
      http_opts_fn_3(v20);
    http_opts_fn_3(a1);
    goto LABEL_11;
  }
}


int http_cookie_fn_18(int *a1, int a2, int a3, int a4, int *a5)
{
  int *v8;
  int v9;
  int v10;
  int *v11;
  int *v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int i;
  int v20;
  int result;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27; // dc
  int v28;
  int v29;

  v8 = NULL;
  v9 = 1;
  v10 = -2143088130;
  if ( a1 != NULL && a5 != NULL )
  {
    *a1 = 0;
    *a5 = 0;
    v11 = &(&HTTP_DATA_U32(0x0))[http_cookie_fn_28(a2)];
    v12 = (int *)v11[11];
    if ( v12 == NULL )
    {
      v13 = *a1;
      goto LABEL_23;
    }
    v14 = http_opts_fn_2(128);
    v8 = (int *)v14;
    v10 = -2143088606;
    if ( v14 != 0 )
    {
      sceKernelMemset(v14, 0, 128);
      while ( 1 )
      {
        v15 = 0;
        v18 = http_io_fn_89();
        if ( v9 == 1 )
        {
          v12 = (int *)v11[11];
          v9 = 0;
        }
        if ( v12 != NULL )
        {
          for ( i = v12[8]; ; i = v12[8] )
          {
            if ( http_cookie_fn_32(i, a2) != 0 )
            {
              v12 = (int *)*v12;
            }
            else
            {
              if ( http_cookie_fn_16((int)v12) >= 0 )
              {
                v15 = 1;
                break;
              }
              v12 = (int *)*v12;
            }
            if ( v12 == NULL )
              break;
          }
        }
        http_io_fn_90(v18, v16, v17);
        if ( v15 != 1 )
          goto LABEL_16;
        v23 = http_cookie_fn_23(a5, v12);
        v10 = v23;
        if ( v23 < 0 )
        {
          http_cookie_fn_17(v12);
          break;
        }
        v24 = v12[3];
        if ( v24 != 0 )
        {
          *(u32 *)(v24 + 8) = v12[2];
          v29 = v12[2];
          if ( v29 == 0 )
          {
            HTTP_DATA_U32(0x28) = v24;
            v12[3] = 0;
            goto LABEL_38;
          }
          *(u32 *)(v29 + 12) = v24;
        }
        else
        {
          HTTP_DATA_U32(0x24) = v12[2];
          v25 = v12[2];
          if ( v25 != 0 )
            *(u32 *)(v25 + 12) = 0;
          else
            HTTP_DATA_U32(0x28) = 0;
        }
        v12[3] = 0;
LABEL_38:
        v26 = HTTP_DATA_U32(0x24);
        v27 = HTTP_DATA_U32(0x24) != 0;
        v12[2] = HTTP_DATA_U32(0x24);
        if ( v27 )
          *(u32 *)(v26 + 12) = v12;
        else
          HTTP_DATA_U32(0x28) = (int)v12;
        v28 = v12[4];
        HTTP_DATA_U32(0x24) = (int)v12;
        http_cookie_fn_31(v8, v28, a3, a4);
LABEL_16:
        if ( v12 == NULL )
        {
          v20 = *v8;
          goto LABEL_20;
        }
        v12 = (int *)*v12;
        if ( v12 == NULL )
        {
          v20 = *v8;
LABEL_20:
          if ( v20 != 0 )
            *a1 = (int)v8;
          v13 = *a1;
LABEL_23:
          v10 = -2143088603;
          result = 0;
          if ( v13 != 0 )
            return result;
          break;
        }
      }
    }
  }
  result = v10;
  if ( a5 != NULL )
  {
    if ( v8 != NULL )
    {
      v22 = *a5;
    }
    else
    {
      if ( *a5 == 0 )
        return v10;
      v22 = *a5;
    }
    http_cookie_fn_19(v8, v22);
    *a5 = 0;
    return v10;
  }
  return result;
}


int http_cookie_fn_19(int a1, int a2)
{
  int result;
  int v4;
  int v5;

  if ( a1 != 0 )
  {
    do
    {
      v4 = *(u32 *)(a1 + 124);
      result = http_opts_fn_3(a1);
      a1 = v4;
    }
    while ( v4 != 0 );
  }
  if ( a2 != 0 )
  {
    do
    {
      v5 = *(u32 *)(a2 + 4);
      http_cookie_fn_17(*(int **)a2);
      result = http_opts_fn_3(a2);
      a2 = v5;
    }
    while ( v5 != 0 );
  }
  return result;
}


int http_cookie_fn_20(u8 *a1, int i, int *a3, unsigned int a4)
{
  int v5;
  int v6;
  int v7;
  int v9;
  int v10;
  int v11;
  int *j;
  unsigned int v13;
  unsigned int v14;
  int v15;
  unsigned int v16;
  int v17;
  int *v18;
  int v20; // dc
  int v21;
  int v22;
  unsigned int v23;
  unsigned int v24;
  int *v25;
  unsigned int v26;

  v5 = 1;
  v6 = 0;
  v7 = 0;
  v25 = a3;
  v26 = a4;
  if ( a1 != NULL )
  {
    v7 = 1;
    *a1 = 0;
  }
  else if ( v25 == NULL )
  {
    return -2143088130;
  }
  v9 = http_io_fn_96(&v23);
  if ( v9 < 0 )
    return v9;
  v10 = *(u32 *)(i + 4);
  if ( v10 != 0 )
  {
    for ( i = *(u32 *)(i + 4); ; i = v10 )
    {
      v10 = *(u32 *)(v10 + 4);
      if ( v10 == 0 )
        break;
    }
  }
  v11 = a1 != NULL;
  if ( i != 0 )
  {
    for ( j = *(int **)i; ; j = *(int **)i )
    {
      v13 = j[13];
      if ( v13 >= v24 )
      {
        if ( v24 != v13 )
        {
          v15 = *j;
          goto LABEL_14;
        }
        v14 = j[12];
        if ( v14 >= v23 )
        {
          v15 = *j;
          goto LABEL_14;
        }
      }
      else
      {
        v14 = j[12];
      }
      if ( (v14 | v13) == 0 )
        break;
      i = *(u32 *)(i + 8);
LABEL_20:
      if ( i == 0 )
      {
        v11 = a1 != NULL;
        goto LABEL_23;
      }
    }
    v15 = *j;
LABEL_14:
    if ( v15 != 0 )
    {
      if ( j[1] != 0 )
      {
        v22 = sceNet_lib_D8722983(v15) + 1;
        v17 = v22 + sceNet_lib_D8722983(j[1]);
LABEL_17:
        v16 = v5 + v17 + 2;
        if ( v7 != 1 )
          goto LABEL_18;
        if ( v26 < v16 )
          return -2143088380;
        v21 = *j;
        if ( *j != 0 )
        {
          if ( j[1] != 0 )
          {
            sceNet_lib_5705F6F9(a1, v21);
            sceNet_lib_5705F6F9(a1, "=");
            v21 = j[1];
          }
        }
        else
        {
          v21 = j[1];
          if ( v21 == 0 )
          {
LABEL_36:
            sceNet_lib_5705F6F9(a1, "; ");
            i = *(u32 *)(i + 8);
            goto LABEL_19;
          }
        }
        sceNet_lib_5705F6F9(a1, v21);
        goto LABEL_36;
      }
    }
    else
    {
      v15 = j[1];
      v16 = v5;
      if ( v15 == 0 )
      {
LABEL_18:
        i = *(u32 *)(i + 8);
LABEL_19:
        v5 = v16;
        ++v6;
        goto LABEL_20;
      }
    }
    v17 = sceNet_lib_D8722983(v15);
    goto LABEL_17;
  }
LABEL_23:
  if ( (v7 & v11) == 0 )
    goto LABEL_24;
  v20 = (unsigned int)sceNet_lib_D8722983(a1) < 3;
  v18 = v25;
  if ( !v20 )
  {
    a1[sceNet_lib_D8722983(a1) - 2] = 0;
LABEL_24:
    v18 = v25;
  }
  if ( v18 != NULL )
    *v18 = v5;
  if ( v6 == 0 )
    return -2143088603;
  return v9;
}


s32 http_cookie_fn_21(int a1, unsigned int a2, int a3, int a4, unsigned int a5, int a6)
{
  int v8;
  unsigned int v9;
  unsigned int v10;
  unsigned int v11;
  int v12;
  int v13;
  int v14;
  s32 v15;
  unsigned int v16;
  int v17;
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
  int *v30;
  int v31;
  int *v32;

  v8 = 0;
  v9 = 0;
  v10 = 0;
  v11 = HTTP_DATA_U32(0x18);
  v25 = a1;
  v26 = a2 != 0;
  v12 = HTTP_DATA_U32(0x18) != 0;
  v27 = a3;
  v28 = a4;
  v29 = a5;
  if ( v26 && v12 && *(u32 *)(a6 + 220) == 1 )
  {
    v20 = v25;
    while ( 1 )
    {
      v21 = sceParseHttpResponseHeader(v20 + v10, a2, "Set-Cookie", &v23, &v24);
      v12 = v9 < v11;
      if ( v21 < 0 )
        break;
      v10 += v21;
      if ( *(u32 *)(a6 + 272) != 0 )
      {
        v22 = http_io_fn_14(a6);
        if ( _sceKernelCheckThreadStack() - 4096 > 0 )
          v30 = &v23;
        v8 = (*(int (**)(u32, int, int, int, u32))(a6 + 272))(
               *(u32 *)(a6 + 24),
               v22,
               v23,
               v24,
               *(u32 *)(a6 + 276));
        if ( v22 != 0 )
          http_opts_fn_3(v22);
      }
      if ( v8 >= 0 )
      {
        ++v9;
        http_cookie_fn_22(v23, v24, v27, v28, v29, 0);
      }
      v12 = v9 < v11;
      if ( v10 >= a2 || !v12 )
        break;
      v20 = v25;
      if ( *(u32 *)(a6 + 220) != 1 )
      {
        v13 = v26;
        goto LABEL_4;
      }
    }
  }
  v13 = v26;
LABEL_4:
  v14 = v13 && !v12;
  v15 = 0;
  v16 = 0;
  if ( v14 != 0 )
  {
    v17 = v25;
    if ( *(u32 *)(a6 + 220) == 1 )
    {
      while ( 1 )
      {
        v19 = sceParseHttpResponseHeader(v17 + v16, a2, "Set-Cookie2", &v23, &v24);
        v15 = v19;
        if ( v19 < 0 )
          break;
        v16 += v19;
        if ( *(u32 *)(a6 + 272) != 0 )
        {
          v31 = http_io_fn_14(a6);
          if ( _sceKernelCheckThreadStack() - 4096 > 0 )
            v32 = &v23;
          v8 = (*(int (**)(u32, int, int, int, u32))(a6 + 272))(
                 *(u32 *)(a6 + 24),
                 v31,
                 v23,
                 v24,
                 *(u32 *)(a6 + 276));
          if ( v31 != 0 )
            http_opts_fn_3(v31);
        }
        if ( v8 >= 0 )
        {
          ++v9;
          http_cookie_fn_22(v23, v24, v27, v28, v29, 2);
          v12 = v9 < v11;
        }
        if ( v16 < a2 && !v12 )
        {
          v17 = v25;
          if ( *(u32 *)(a6 + 220) == 1 )
            continue;
        }
        return http_min_s32(v15, 0);
      }
      v15 = -2143088603;
    }
  }
  return http_min_s32(v15, 0);
}


int http_cookie_fn_22(int a1, int a2, int a3, int a4, unsigned int a5, int a6)
{
  int v8;
  int v10;
  int result;
  u32 *v12;
  u32 *v13;
  u32 *v14;
  u32 *v15;
  int v16;
  int v17;
  u32 *v18;
  unsigned int v19;
  int v20[3];

  v8 = a5;
  v18 = NULL;
  v15 = NULL;
  v19 = 0;
  v20[0] = 0;
  v10 = http_cookie_fn_26(a1, a2, &v15, &v16, &v17);
  if ( v10 < 0 || (v10 = http_cookie_fn_24(&v18, &v19, v20, v15, a3, a4, v8, v16, v17, a6)) < 0 )
  {
LABEL_10:
    v12 = v15;
    goto LABEL_11;
  }
  if ( v18 == NULL )
  {
LABEL_4:
    if ( v15 != NULL )
      http_cookie_fn_11(v15);
    return 0;
  }
  http_cookie_fn_30(v19, v20[0]);
  v12 = v15;
  if ( HTTP_DATA_U32(0x8) - HTTP_DATA_U32(0xC) >= v19 )
  {
    v10 = http_cookie_fn_13(v18, v19, v20[0]);
    if ( v10 >= 0 )
      goto LABEL_4;
    goto LABEL_10;
  }
LABEL_11:
  if ( v12 != NULL )
    http_cookie_fn_11(v12);
  v13 = v18;
  result = v10;
  if ( v18 != NULL )
  {
    do
    {
      http_cookie_fn_12(v13);
      v14 = (u32 *)v13[16];
      http_opts_fn_3(v13);
      v13 = v14;
    }
    while ( v14 != NULL );
    return v10;
  }
  return result;
}


int http_cookie_fn_23(int *a1, int a2)
{
  u32 *v4;
  int result;
  int v6;

  v4 = (u32 *)http_opts_fn_2(8);
  result = -2143088606;
  if ( v4 != NULL )
  {
    v6 = *a1;
    *v4 = a2;
    v4[1] = v6;
    *a1 = (int)v4;
    return 0;
  }
  return result;
}


int http_cookie_fn_24(
        u32 *a1,
        int a2,
        u32 *a3,
        u32 *a4,
        u8 *a5,
        int a6,
        unsigned int a7,
        int a8,
        int a9,
        int a10)
{
  u32 *v10;
  u32 *v13;
  int v14;
  int v15;
  int *v16;
  s64 v17;
  int v18;
  u8 *v19;
  int result;
  int v21;
  u32 *v22;
  int v23;
  u8 *v24;
  int v25;
  int *v26;
  u8 *v27;
  u8 *v28;
  int v29;
  u8 *v30;
  int v31;
  u8 *v32;
  int v33;
  u32 *v34;
  u32 *v35;
  u8 *v36;
  u32 *v37;
  int v38; // dc
  int v39;
  int v40;
  int v41;
  int v42;
  int v43;
  int v44;
  char v45[16];
  int v46;
  int v47;
  int v48;
  u32 *v49;
  u32 *v50;
  u8 *v51;
  int v52;
  int v53;
  int *v54;
  int v55;
  int v56;
  u8 *v57;
  u8 *v58;
  int v59;
  u8 *v60;
  u8 *v61;
  int v62;
  int v63;
  s64 v64;
  int v65;

  v10 = a4;
  v51 = a5;
  v50 = a1;
  v52 = a8;
  v46 = 0;
  v47 = 0;
  v56 = a10;
  v13 = NULL;
  v14 = 0;
  v15 = 0;
  v53 = a7;
  v49 = NULL;
  v48 = 0;
  v54 = NULL;
  v55 = a9;
  v57 = NULL;
  v58 = NULL;
  v59 = 0;
  v60 = NULL;
  v61 = NULL;
  v62 = 0;
  v63 = 0;
  v64 = 0;
  v16 = NULL;
  if ( a1 == NULL || a2 == 0 || ((a3 == NULL) || (a4 == NULL)) )
  {
    v15 = -2143088130;
LABEL_5:
    v18 = v48;
    goto LABEL_6;
  }
  *(u32 *)a2 = 0;
  *a3 = 0;
  v21 = http_ssl_fn_2(a6);
  v14 = v21;
  if ( v21 == 0 )
  {
    v15 = -2143088606;
    goto LABEL_5;
  }
  if ( v10 == NULL )
  {
    v59 = v21;
    goto LABEL_31;
  }
  v65 = (unsigned int)(v56 - 1) < 2;
  do
  {
    v22 = (u32 *)v10[2];
    v23 = *v10;
    v24 = (u8 *)v10[1];
    if ( v22 == NULL )
    {
      v40 = http_opts_fn_2(12);
      v16 = (int *)v40;
      if ( v40 == 0 )
        goto LABEL_61;
      sceKernelMemset(v40, 0, 12);
      if ( v54 != NULL )
        v16[2] = (int)v54;
      v54 = v16;
      if ( v23 != 0 )
      {
        v41 = sceNet_lib_D8722983(v23);
        v42 = http_opts_fn_2(v41 + 1);
        *v16 = v42;
        if ( v42 == 0 )
          goto LABEL_61;
        sceNet_lib_80C9F02A(v42, v23);
      }
      if ( v24 != NULL )
      {
        v43 = sceNet_lib_D8722983(v24);
        v44 = http_opts_fn_2(v43 + 1);
        v16[1] = v44;
        if ( v44 == 0 )
          goto LABEL_61;
        sceNet_lib_80C9F02A(v44, v24);
        v22 = (u32 *)v10[2];
      }
      else
      {
        v22 = (u32 *)v10[2];
      }
      goto LABEL_28;
    }
    if ( v23 == 0 )
      goto LABEL_28;
    if ( v65 )
    {
      if ( sceNet_lib_9CFBC7E3(*v10, "Comment") == 0 )
      {
        v22 = (u32 *)v10[2];
        v60 = v24;
        goto LABEL_28;
      }
      if ( sceNet_lib_9CFBC7E3(v23, "Max-Age") == 0 )
      {
        if ( v24 != NULL )
        {
          v64 = http_io_fn_40(v24);
          v15 = http_io_fn_96(&v46);
          if ( v15 < 0 )
            goto LABEL_55;
          v15 = http_io_fn_97(&v46, &v46, v64, HIDWORD(v64));
          if ( v15 < 0 )
          {
            v26 = v54;
            goto LABEL_56;
          }
          goto LABEL_63;
        }
        v22 = (u32 *)v10[2];
        goto LABEL_28;
      }
      v39 = sceNet_lib_9CFBC7E3(v23, "Version");
      v25 = v56;
      if ( v39 == 0 )
      {
        if ( v24 != NULL )
        {
          v62 = sceNet_lib_1FB2FDDD(v24);
LABEL_63:
          v22 = (u32 *)v10[2];
          goto LABEL_28;
        }
        v22 = (u32 *)v10[2];
        goto LABEL_28;
      }
    }
    else
    {
      v25 = v56;
    }
    if ( v25 == 2 )
    {
      if ( sceNet_lib_9CFBC7E3(v23, "CommentURL") == 0 )
      {
        v22 = (u32 *)v10[2];
        v61 = v24;
        goto LABEL_28;
      }
      if ( sceNet_lib_9CFBC7E3(v23, "Port") != 0 )
      {
LABEL_22:
        if ( sceNet_lib_9CFBC7E3(v23, "Domain") != 0 )
        {
          v38 = sceNet_lib_9CFBC7E3(v23, "Path") != 0;
          v22 = (u32 *)v10[2];
          if ( !v38 && v24 != NULL )
            v59 = (int)v24;
        }
        else if ( v24 != NULL )
        {
          if ( http_cookie_fn_27(v24) == 0 )
          {
            v26 = v54;
            goto LABEL_56;
          }
          v22 = (u32 *)v10[2];
          v58 = v24;
        }
        else
        {
          v22 = (u32 *)v10[2];
        }
        goto LABEL_28;
      }
      if ( v24 == NULL )
        goto LABEL_63;
      v15 = http_ssl_fn_1(v24, v53, &v48);
      if ( v15 <= 0 )
      {
        v15 = -2143088603;
        goto LABEL_55;
      }
      v22 = (u32 *)v10[2];
      v63 = v15;
    }
    else
    {
      if ( v56 != 0 || sceNet_lib_9CFBC7E3(v23, "expires") != 0 )
        goto LABEL_22;
      if ( v24 == NULL )
        goto LABEL_63;
      if ( v64 != 0 )
      {
        v22 = (u32 *)v10[2];
      }
      else
      {
        v15 = http_io_fn_98(&v46, v24);
        if ( v15 >= 0 )
          goto LABEL_63;
        v22 = (u32 *)v10[2];
        v46 = 0;
        v47 = 0;
      }
    }
LABEL_28:
    v10 = v22;
  }
  while ( v22 != NULL );
  if ( v59 == 0 )
    v59 = v14;
LABEL_31:
  v27 = v58;
  if ( v58 == NULL )
    v27 = v51;
  v28 = v27;
  if ( *v27 == 46 )
  {
LABEL_37:
    v30 = v51;
    goto LABEL_38;
  }
  v29 = http_io_fn_95(2, v27, v45);
  v30 = v51;
  if ( v29 == 1 )
  {
LABEL_38:
    v33 = http_cookie_fn_32(v28, v30);
    v26 = v54;
    if ( v33 != 0 )
      goto LABEL_56;
    if ( v16 != NULL )
    {
      while ( 1 )
      {
        v15 = http_cookie_fn_25(&v49, *v16, v16[1], v60, v61, v28, v59, v48, v63, &v46, v62, v52, v55, v56);
        if ( v15 < 0 )
          goto LABEL_55;
        v35 = v49;
        ++*a3;
        *(u32 *)a2 += v35[14];
        if ( v13 != NULL )
          v35[16] = v13;
        v16 = (int *)v16[2];
        v13 = v35;
        if ( v16 == NULL )
        {
          v34 = v50;
          v26 = v54;
          goto LABEL_46;
        }
      }
    }
    v34 = v50;
LABEL_46:
    *v34 = v13;
    if ( v26 != NULL )
      http_cookie_fn_11(v26);
    if ( v48 != 0 )
      http_opts_fn_3(v48);
    v36 = v57;
    if ( v14 != 0 )
    {
      http_opts_fn_3(v14);
      v36 = v57;
    }
    if ( v36 != NULL )
      http_opts_fn_3(v36);
    return *(u32 *)a2;
  }
  v31 = sceNet_lib_D8722983(v28);
  v57 = (u8 *)http_opts_fn_2(v31 + 2);
  if ( v57 != NULL )
  {
    v32 = v57;
    *v57 = 46;
    sceNet_lib_80C9F02A(v32 + 1, v28);
    v28 = v57;
    goto LABEL_37;
  }
LABEL_61:
  v15 = -2143088606;
LABEL_55:
  v26 = v54;
LABEL_56:
  if ( v26 != NULL )
    http_cookie_fn_11(v26);
  v18 = v48;
  if ( v13 != NULL )
  {
    do
    {
      http_cookie_fn_12(v13);
      v37 = (u32 *)v13[16];
      http_opts_fn_3(v13);
      v13 = v37;
    }
    while ( v37 != NULL );
    v18 = v48;
  }
LABEL_6:
  if ( v18 != 0 )
    http_opts_fn_3(v18);
  v19 = v57;
  if ( v14 != 0 )
  {
    http_opts_fn_3(v14);
    v19 = v57;
  }
  result = v15;
  if ( v19 != NULL )
  {
    http_opts_fn_3(v19);
    return v15;
  }
  return result;
}


int http_cookie_fn_25(
        int **a1,
        int a2,
        int a3,
        int a4,
        int a5,
        int a6,
        int a7,
        int a8,
        int a9,
        int *a10,
        int a11,
        int a12,
        int a13,
        int a14)
{
  unsigned int v20;
  int *v21;
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

  v20 = 0;
  v21 = (int *)http_opts_fn_2(72);
  v22 = -2143088606;
  if ( v21 != NULL )
  {
    sceKernelMemset(v21, 0, 72);
    if ( a2 != 0 )
    {
      v20 = sceNet_lib_D8722983(a2) + 1;
      v22 = -2143088380;
      if ( HTTP_DATA_U32(0x10) < v20 )
        goto LABEL_33;
      v23 = sceNet_lib_D8722983(a2);
      v24 = http_opts_fn_2(v23 + 1);
      *v21 = v24;
      v22 = -2143088606;
      if ( v24 == 0 )
        goto LABEL_33;
      sceNet_lib_80C9F02A(v24, a2);
    }
    if ( a3 != 0 )
    {
      v20 += sceNet_lib_D8722983(a3) + 1;
      v22 = -2143088380;
      if ( HTTP_DATA_U32(0x10) < v20 )
        goto LABEL_33;
      v25 = sceNet_lib_D8722983(a3);
      v26 = http_opts_fn_2(v25 + 1);
      v21[1] = v26;
      v22 = -2143088606;
      if ( v26 == 0 )
        goto LABEL_33;
      sceNet_lib_80C9F02A(v26, a3);
    }
    if ( a4 != 0 )
    {
      v20 += sceNet_lib_D8722983(a4) + 1;
      v22 = -2143088380;
      if ( HTTP_DATA_U32(0x10) + HTTP_DATA_U32(0x14) < v20 )
        goto LABEL_33;
      v27 = sceNet_lib_D8722983(a4);
      v28 = http_opts_fn_2(v27 + 1);
      v21[4] = v28;
      v22 = -2143088606;
      if ( v28 == 0 )
        goto LABEL_33;
      sceNet_lib_80C9F02A(v28, a4);
    }
    if ( a5 != 0 )
    {
      v20 += sceNet_lib_D8722983(a5) + 1;
      v22 = -2143088380;
      if ( HTTP_DATA_U32(0x10) + HTTP_DATA_U32(0x14) < v20 )
        goto LABEL_33;
      v29 = sceNet_lib_D8722983(a5);
      v30 = http_opts_fn_2(v29 + 1);
      v21[5] = v30;
      v22 = -2143088606;
      if ( v30 == 0 )
        goto LABEL_33;
      sceNet_lib_80C9F02A(v30, a5);
    }
    if ( a6 != 0 )
    {
      v20 += sceNet_lib_D8722983(a6) + 1;
      v22 = -2143088380;
      if ( HTTP_DATA_U32(0x10) + HTTP_DATA_U32(0x14) < v20 )
        goto LABEL_33;
      v31 = sceNet_lib_D8722983(a6);
      v32 = http_opts_fn_2(v31 + 1);
      v21[2] = v32;
      v22 = -2143088606;
      if ( v32 == 0 )
        goto LABEL_33;
      sceNet_lib_80C9F02A(v32, a6);
    }
    if ( a7 != 0 )
    {
      v20 += sceNet_lib_D8722983(a7) + 1;
      v22 = -2143088380;
      if ( HTTP_DATA_U32(0x10) + HTTP_DATA_U32(0x14) < v20 )
        goto LABEL_33;
      v34 = sceNet_lib_D8722983(a7);
      v35 = http_opts_fn_2(v34 + 1);
      v21[3] = v35;
      v22 = -2143088606;
      if ( v35 == 0 )
        goto LABEL_33;
      v36 = sceNet_lib_D8722983(a7);
      v22 = sceUriSweepPath(v21[3], a7, v36 + 1);
      if ( v22 < 0 )
        goto LABEL_33;
      v33 = a8;
    }
    else
    {
      v33 = a8;
    }
    if ( v33 == 0 )
    {
LABEL_32:
      v38 = *a10;
      v39 = a10[1];
      *a1 = v21;
      v21[12] = v38;
      v21[13] = v39;
      v21[11] = a11;
      v21[6] = a12;
      v21[7] = a13;
      v21[15] = a14;
      v21[14] = v20;
      return 0;
    }
    v20 += 2 * a9;
    v22 = -2143088380;
    if ( HTTP_DATA_U32(0x10) + HTTP_DATA_U32(0x14) >= v20 )
    {
      v37 = http_opts_fn_2(2 * a9);
      v21[9] = v37;
      v22 = -2143088606;
      if ( v37 != 0 )
      {
        sceKernelMemcpy(v37, a8, 2 * a9);
        v21[10] = a9;
        goto LABEL_32;
      }
    }
  }
LABEL_33:
  if ( v21 != NULL )
  {
    http_cookie_fn_12(v21);
    http_opts_fn_3(v21);
  }
  return v22;
}


int http_cookie_fn_26(u8 *a1, unsigned int a2, int **a3, u32 *a4, u32 *a5, int a6)
{
  unsigned int v8;
  int *v9;
  int v10;
  u8 *v11;
  int v12;
  int v13;
  int v15;
  unsigned int v16;
  int v17;
  s64 v18;
  int *v19;
  int v20;
  int v21;
  char v22;
  unsigned int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  u8 *j;
  u8 *i;
  int *v30;
  int v31;
  unsigned int v32;
  int *v37;
  int v38;

  v8 = 0;
  v9 = NULL;
  *a4 = 0;
  *a5 = 0;
  v37 = NULL;
  if ( a2 == 0 )
    goto LABEL_12;
  v10 = true;
  v11 = a1;
  while ( 1 )
  {
    v38 = 0;
    v12 = 0;
    v13 = 0;
    if ( http_cookie_fn_29(v11) != 0 )
    {
      *a4 = 1;
      if ( *v11 != 59 )
      {
        ++v8;
        while ( 1 )
        {
          v10 = v8 < a2;
          if ( v8 >= a2 )
            goto LABEL_12;
          if ( a1[v8] == 59 )
            goto LABEL_9;
          ++v8;
        }
      }
      goto LABEL_9;
    }
    v15 = a2 - v8;
    if ( a6 == 2 && http_cookie_fn_29(v11) != 0 )
      break;
    while ( v10 )
    {
      v16 = v8;
      if ( (sceNet_lib_750F705D((char)a1[v8], v15) & 8) == 0 )
        goto LABEL_18;
      v10 = ++v8 < a2;
    }
    while ( 1 )
    {
      v16 = v8 + v12;
LABEL_18:
      if ( v16 >= a2 )
        break;
      v17 = (char)a1[v16];
      v15 = v17 ^ 0x3B;
      v18 = ((u64)(v18) & 0xFFFFFFFF00000000ULL) | (u32)(v17 == 0x3D);
      v18 = ((u64)(v18) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v17 == 0x3B) << 32);
      if ( v18 != 0 )
        break;
      ++v12;
    }
    if ( v16 == a2 )
    {
      v19 = v37;
    }
    else
    {
      v19 = v37;
      if ( a1[v16] != 59 )
        goto LABEL_23;
    }
    v38 = 1;
    if ( v19 != NULL )
    {
      v32 = a2;
      if ( v16 != a2 )
        v32 = v16 + 1;
      v8 = v32;
      goto LABEL_47;
    }
LABEL_23:
    v20 = http_opts_fn_2(12);
    v9 = (int *)v20;
    if ( v20 == 0 )
      goto LABEL_65;
    sceKernelMemset(v20, 0, 12);
    v9[2] = (int)v37;
    if ( v12 != 0 )
    {
      v31 = http_opts_fn_2(v12 + 1);
      *v9 = v31;
      if ( v31 == 0 )
      {
LABEL_63:
        v30 = v9;
        if ( v9 == NULL )
        {
LABEL_65:
          v30 = v37;
          if ( v37 == NULL )
            return -2143088606;
        }
        http_cookie_fn_11(v30);
        return -2143088606;
      }
      sceKernelMemcpy(v31, &a1[v8], v12);
      *(u8 *)(*v9 + v12) = 0;
    }
    v21 = v16 ^ a2;
    if ( v38 != 1 )
    {
      ++v16;
      if ( v21 == 0 )
        v16 = a2;
      while ( 1 )
      {
        v15 = v16 < a2;
        if ( v16 >= a2 )
          break;
        v22 = sceNet_lib_750F705D((char)a1[v16], v15);
        v23 = v16;
        if ( (v22 & 8) == 0 )
          goto LABEL_32;
        ++v16;
      }
      while ( 1 )
      {
        v23 = v16 + v13;
LABEL_32:
        if ( v23 >= a2 || a1[v23] == 59 )
          break;
        ++v13;
      }
      if ( v13 != 0 )
        goto LABEL_37;
    }
    v8 = v16 + v13;
    if ( v12 != 0 && v38 == 0 )
    {
LABEL_37:
      v24 = http_opts_fn_2(v13 + 1);
      v9[1] = v24;
      if ( v24 == 0 )
        goto LABEL_63;
      sceKernelMemcpy(v24, &a1[v16], v13);
      *(u8 *)(v9[1] + v13) = 0;
      v8 = v16 + v13;
    }
    if ( v8 < a2 )
    {
      if ( a1[v8] == 59 )
        v8 = v16 + 1 + v13;
      v25 = *v9;
    }
    else
    {
      v25 = *v9;
    }
    if ( v25 != 0 )
    {
      if ( sceNet_lib_D8722983(v25) != 0 )
      {
        for ( i = (u8 *)(*v9 + sceNet_lib_D8722983(*v9)); (sceNet_lib_750F705D((char)*--i, v15) & 8) != 0; *i = 0 )
          ;
        v26 = v9[1];
      }
      else
      {
        v26 = v9[1];
      }
    }
    else
    {
      v26 = v9[1];
    }
    if ( v26 == 0 )
      goto LABEL_47;
    v10 = v8 < a2;
    if ( sceNet_lib_D8722983(v26) != 0 )
    {
      v27 = sceNet_lib_D8722983(v9[1]);
      v15 = v9[1];
      for ( j = (u8 *)(v15 + v27); ; *j = 0 )
      {
        --j;
        v10 = v8 < a2;
        if ( (sceNet_lib_750F705D((char)*j, v15) & 8) == 0 )
          break;
      }
    }
    while ( 2 )
    {
      if ( !v10 )
      {
        v37 = v9;
        goto LABEL_11;
      }
      if ( (sceNet_lib_750F705D((char)a1[v8], v15) & 8) != 0 )
      {
        ++v8;
LABEL_47:
        v10 = v8 < a2;
        continue;
      }
      break;
    }
    v37 = v9;
LABEL_11:
    v11 = &a1[v8];
    if ( !v10 )
      goto LABEL_12;
  }
  *a5 = 1;
  if ( *v11 == 59 )
  {
LABEL_9:
    ++v8;
    if ( !v10 )
      goto LABEL_12;
    v10 = v8 < a2;
    goto LABEL_11;
  }
  ++v8;
  while ( 1 )
  {
    v10 = v8 < a2;
    if ( v8 >= a2 )
      break;
    if ( a1[v8] == 59 )
      goto LABEL_9;
    ++v8;
  }
LABEL_12:
  *a3 = v9;
  return 0;
}


int http_cookie_fn_27(int a1)
{
  int v2;
  unsigned int v3;
  int v5;
  int v6;
  int v7;

  v2 = http_ssl_fn_5(a1, &v5, &v6, &v7);
  if ( v2 < 0 )
    return 0;
  v3 = http_ssl_fn_3(v5, v6, v7);
  if ( v2 < v3 )
    return 0;
  else
    return http_ssl_fn_4(a1, v3);
}


int http_cookie_fn_28(int a1)
{
  char *v2;
  s64 v3;
  u8 v5[16];

  v2 = (char *)http_cookie_fn_27(a1);
  if ( v2 == NULL )
    v2 = (char *)a1;
  v3 = ((u64)(v3) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(sceNet_lib_D8722983(v2)) << 32);
  if ( HIDWORD(v3) != 0 )
  {
    v3 = ((u64)(v3) & 0xFFFFFFFF00000000ULL) | (u32)(v2[HIDWORD(v3) - 1] ^ 0x2E);
    v3 = ((u64)(v3) & 0x00000000FFFFFFFFULL) | ((u64)(u32)((u64)(v3 - 1) >> 32) << 32);
    if ( HIDWORD(v3) != 0 )
      v2 += *v2 == 0x2E;
  }
  http_io_fn_91(v2, HIDWORD(v3), v5);
  return v5[0] >> 4;
}


int http_cookie_fn_29(int a1, unsigned int a2, int a3)
{
  unsigned int v5;
  unsigned int v7;
  int result;
  int v9; // dc
  int v10;
  char *v11;
  int v12;

  v5 = sceNet_lib_D8722983(a3);
  v7 = v5;
  if ( a2 < v5 )
    return false;
  v9 = sceNet_lib_B9085A96(a1, a3, v5) != 0;
  result = true;
  if ( v9 )
    return false;
  v11 = (char *)(a1 + v7);
  if ( v7 != a2 )
  {
    v12 = *v11;
    if ( v12 != 59 )
      return (sceNet_lib_750F705D(v12, v10) & 8) != 0;
  }
  return result;
}


int http_cookie_fn_30(unsigned int a1, unsigned int a2)
{
  int result;
  unsigned int v3;
  unsigned int v4;
  int v5;
  int v6;
  int v7;
  int *v8;

  result = HTTP_DATA_U32(0x8);
  v3 = 0;
  if ( HTTP_DATA_U32(0x8) - HTTP_DATA_U32(0xC) < a1 || HTTP_DATA_U32(0x1C) - HTTP_DATA_U32(0x20) < a2 )
  {
    v4 = 10;
    if ( a2 >= 0xB )
      v4 = a2;
    v7 = http_io_fn_89();
    v8 = (int *)HTTP_DATA_U32(0x28);
    do
    {
      ++v3;
      if ( v8 == NULL )
        break;
      if ( http_cookie_fn_16((int)v8) >= 0 )
      {
        v8[5] = 1;
        HTTP_DATA_U32(0x20) -= v8[7];
        http_cookie_fn_17(v8);
      }
      v8 = (int *)v8[3];
    }
    while ( v3 < v4 );
    return http_io_fn_90(v7, v5, v6);
  }
  return result;
}


int http_cookie_fn_31(int a1, u32 *a2, char *a3, int a4)
{
  int i;
  int v8;
  int v10;
  int v11;
  char *v12;
  int v13;
  int v14;
  int v15;

  i = a1;
  v8 = *(u32 *)(a1 + 124);
  if ( v8 != 0 )
  {
    for ( i = *(u32 *)(a1 + 124); ; i = v8 )
    {
      v8 = *(u32 *)(v8 + 124);
      if ( v8 == 0 )
        break;
    }
  }
  while ( 1 )
  {
    if ( a2 == NULL )
      return 0;
    if ( *(int *)(i + 120) >= 10 )
      break;
LABEL_8:
    if ( a2[8] == 1 )
    {
      a2 = (u32 *)a2[16];
    }
    else
    {
      v10 = a2[3];
      if ( a3 != NULL && sceNet_lib_D8722983(a3) != 0 )
      {
        v11 = sceNet_lib_D8722983(v10);
        v13 = v10;
        v12 = a3;
      }
      else
      {
        v11 = sceNet_lib_D8722983(v10);
        v12 = "/";
        v13 = v10;
      }
      if ( sceNet_lib_B9085A96(v13, v12, v11) != 0 )
      {
        a2 = (u32 *)a2[16];
      }
      else
      {
        if ( a2[6] != 1 || a4 != 0 )
        {
          *(u32 *)(12 * *(u32 *)(i + 120) + i) = a2;
          http_ssl_fn_6();
          ++*(u32 *)(i + 120);
        }
        a2 = (u32 *)a2[16];
      }
    }
  }
  v14 = http_opts_fn_2(128);
  v15 = v14;
  if ( v14 != 0 )
  {
    sceKernelMemset(v14, 0, 128);
    *(u32 *)(i + 124) = v15;
    i = v15;
    goto LABEL_8;
  }
  return -2143088606;
}


int http_cookie_fn_32(u8 *a1, int a2)
{
  int v4;
  int v5;
  int v6;
  int v7;
  int result;
  unsigned int v9;
  unsigned int v10;
  u8 *v11;
  int v12;
  unsigned int v13;
  int v14;

  v5 = sceNet_lib_D8722983(a1);
  v4 = sceNet_lib_D8722983(a2);
  v6 = v5 == 0 || v4 == 0;
  v7 = v4;
  result = -1;
  if ( !v6 )
  {
    v9 = (__PAIR64__(v5, (char)a1[v5 - 1] ^ 0x2Eu) - 1) >> 32;
    v10 = (__PAIR64__(v7, *(char *)(a2 + v7 - 1) ^ 0x2Eu) - 1) >> 32;
    if ( http_io_fn_95(2, a2, &v14) != 1 && *a1 == 46 )
    {
      v11 = a1 + 1;
      v12 = a2;
      v13 = v10;
      if ( v9 - 1 == v10 )
        return sceNet_lib_B9085A96(v11, v12, v13);
      result = -1;
      v12 = a2 + v10 - v9;
      v11 = a1;
      if ( v10 < v9 )
        return result;
    }
    else
    {
      result = -1;
      if ( v9 != v10 )
        return result;
      v11 = a1;
      v12 = a2;
    }
    v13 = v9;
    return sceNet_lib_B9085A96(v11, v12, v13);
  }
  return result;
}


