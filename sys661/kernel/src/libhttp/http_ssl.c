#include "libhttp_internal.h"

int http_ssl_fn_1(int a1, unsigned int a2, int *a3)
{
  int v3;
  int v6;
  int v7;
  int result;

  *a3 = 0;
  v3 = a2;
  v6 = http_ssl_fn_7(a1, a2, 0);
  if ( v6 > 0 )
  {
    v7 = http_opts_fn_2(2 * v6);
    *a3 = v7;
    v6 = -2143088606;
    if ( v7 != 0 )
    {
      v6 = http_ssl_fn_7(a1, v3, v7);
      if ( v6 > 0 )
        return v6;
    }
  }
  result = v6;
  if ( *a3 != 0 )
  {
    http_opts_fn_3(*a3);
    *a3 = 0;
    return v6;
  }
  return result;
}


int http_ssl_fn_2(char *a1)
{
  char *v1;
  int v2;
  int v3;
  int v4;

  v1 = a1;
  v2 = sceNet_lib_F48963C6(a1, 47);
  v3 = v2 - (u32)v1 + 1;
  if ( v2 == 0 )
  {
    v1 = "/";
    v3 = sceNet_lib_D8722983("/");
  }
  v4 = http_opts_fn_2(v3 + 1);
  if ( v4 != 0 )
  {
    sceKernelMemcpy(v4, v1, v3);
    *(u8 *)(v4 + v3) = 0;
  }
  return v4;
}


int http_ssl_fn_3(int a1, int a2, unsigned int a3)
{
  int result;
  int v5; // dc

  result = 2;
  if ( a3 < 3 )
  {
    result = 3;
    if ( a2 == 3 )
    {
      if ( sceNet_lib_B9085A96(a1, "com", 3) == 0 || sceNet_lib_B9085A96(a1, "edu", 3) == 0 )
        return 2;
      v5 = sceNet_lib_B9085A96(a1, "net", 3) == 0;
      result = 2;
      if ( !v5 )
      {
        v5 = sceNet_lib_B9085A96(a1, "org", 3) == 0;
        result = 2;
        if ( !v5 )
        {
          v5 = sceNet_lib_B9085A96(a1, "gov", 3) == 0;
          result = 2;
          if ( !v5 )
          {
            if ( sceNet_lib_B9085A96(a1, "mil", 3) == 0 )
              return 2;
            v5 = sceNet_lib_B9085A96(a1, "int", 3) == 0;
            result = 3;
            if ( v5 )
              return 2;
          }
        }
      }
    }
  }
  return result;
}


int http_ssl_fn_4(int a1, int a2)
{
  int v4; // dc
  int result;
  int v6;
  int v7;
  int v8;

  if ( a1 == 0 )
    return 0;
  v4 = sceNet_lib_D8722983(a1) == 0;
  result = 0;
  if ( v4 )
    return result;
  if ( a2 == 0 )
    return 0;
  v6 = a1 + sceNet_lib_D8722983(a1) - 1;
  v7 = v6 ^ a1;
  v8 = a2 != 0;
  while ( v7 != 0 && v8 )
  {
    if ( a2 != 0 )
      v7 = --v6 ^ a1;
    if ( *(u8 *)v6 == 46 )
      v8 = --a2 != 0;
  }
  result = v6 + 1;
  if ( a2 != 0 )
  {
    if ( a2 != 1 )
      return 0;
    result = v6;
    if ( *(u8 *)v6 == 46 )
      return 0;
  }
  return result;
}


int http_ssl_fn_5(unsigned int a1, unsigned int *a2, int *a3, u32 *a4)
{
  s64 v8;
  int result;
  int v10; // dc
  u8 *v11;
  unsigned int v12;
  int v13;
  char i;
  int v15;
  int v16;
  int j;

  if ( a1 == 0 || a2 == NULL )
    return -1;
  v8 = ((u64)(v8) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(a3 == NULL) << 32);
  v8 = ((u64)(v8) & 0xFFFFFFFF00000000ULL) | (u32)(a4 == NULL);
  if ( v8 != 0 )
    return -1;
  v10 = sceNet_lib_D8722983(a1) == 0;
  result = -1;
  if ( !v10 )
  {
    *a3 = 0;
    *a4 = 0;
    v11 = (u8 *)(a1 + sceNet_lib_D8722983(a1));
    v12 = (unsigned int)(v11 - 1);
    v13 = v12 ^ a1;
    if ( *(u8 *)v12 == 0x2E && v12 != a1 )
    {
      v12 = (unsigned int)(v11 - 2);
      if ( *(v11 - 2) != 0x2E && (unsigned int)(v11 - 2) != a1 )
      {
        v15 = *a3;
        goto LABEL_11;
      }
      i = *(u8 *)v12;
    }
    else
    {
      for ( i = *(u8 *)v12; i != 0x2E && v13 != 0; i = *(u8 *)v12 )
      {
        v15 = *a3;
LABEL_11:
        v13 = --v12 ^ a1;
        *a3 = v15 + 1;
      }
    }
    if ( i == 46 )
    {
      *a2 = v12 + 1;
    }
    else
    {
      v16 = *a3;
      *a2 = v12;
      *a3 = v16 + 1;
    }
    j = 1;
    if ( v12 != a1 )
    {
      while ( *(u8 *)--v12 != 46 )
      {
        ++*a4;
        if ( v12 == a1 )
          return j + (*(u8 *)a1 != 0x2E);
      }
      for ( j = 2; ; j += *(char *)v12 == 0x2E )
      {
        v10 = v12-- == a1;
        if ( v10 )
          break;
      }
    }
    return j + (*(u8 *)a1 != 0x2E);
  }
  return result;
}


void http_ssl_fn_6(int *a1, int *a2)
{
  int *v2;
  int v4;
  int v5;
  int v6;
  int v7;
  int *i;
  int v9;
  int *v10;
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

  v2 = a1;
  if ( a1 != a2 )
  {
    v4 = *a1;
    v5 = *a2;
    v6 = sceNet_lib_D8722983(*(u32 *)(*a1 + 12));
    v7 = v6 - sceNet_lib_D8722983(*(u32 *)(v5 + 12));
    if ( v7 == 0 )
    {
      v23 = sceNet_lib_D8722983(*(u32 *)(v4 + 8));
      v7 = v23 - sceNet_lib_D8722983(*(u32 *)(v5 + 8));
    }
    if ( v7 < 0 )
    {
      for ( i = (int *)v2[1]; ; i = (int *)v2[1] )
      {
        if ( i == NULL )
        {
          a2[1] = 0;
          goto LABEL_28;
        }
        v18 = *i;
        v19 = *a2;
        v20 = sceNet_lib_D8722983(*(u32 *)(v18 + 12));
        v21 = v20 - sceNet_lib_D8722983(*(u32 *)(v19 + 12));
        if ( v21 == 0 )
        {
          v22 = sceNet_lib_D8722983(*(u32 *)(v18 + 8));
          v21 = v22 - sceNet_lib_D8722983(*(u32 *)(v19 + 8));
        }
        if ( v21 >= 0 )
          break;
        v2 = (int *)v2[1];
      }
      a2[1] = v2[1];
LABEL_28:
      v2[1] = (int)a2;
      v17 = a2[1];
      a2[2] = (int)v2;
      goto LABEL_18;
    }
    if ( v7 <= 0 )
    {
      v9 = v2[1];
      a2[2] = (int)v2;
      a2[1] = v9;
      v2[1] = (int)a2;
      v17 = a2[1];
LABEL_18:
      if ( v17 != 0 )
        *(u32 *)(v17 + 8) = a2;
      return;
    }
    while ( 1 )
    {
      v10 = (int *)v2[2];
      if ( v10 == NULL )
      {
        a2[2] = 0;
        goto LABEL_15;
      }
      v11 = *v10;
      v12 = *a2;
      v13 = sceNet_lib_D8722983(*(u32 *)(v11 + 12));
      v14 = v13 - sceNet_lib_D8722983(*(u32 *)(v12 + 12));
      if ( v14 == 0 )
      {
        v16 = sceNet_lib_D8722983(*(u32 *)(v11 + 8));
        v14 = v16 - sceNet_lib_D8722983(*(u32 *)(v12 + 8));
      }
      if ( v14 <= 0 )
        break;
      v2 = (int *)v2[2];
    }
    a2[2] = v2[2];
LABEL_15:
    v2[2] = (int)a2;
    v15 = a2[2];
    a2[1] = (int)v2;
    if ( v15 != 0 )
      *(u32 *)(v15 + 4) = a2;
  }
}


int http_ssl_fn_7(char *a1, int a2, int a3)
{
  char *v3;
  int v5;
  int v6;
  int v7;
  char v8;
  int v9;
  int v10;
  int result;
  unsigned int v12;
  char v13;
  char *v14;
  int v15;
  u8 v16[16];

  v3 = a1;
  v5 = (u16)a2;
  v6 = 0;
  v7 = 0;
  v8 = *a1;
  v9 = a3 != 0;
  if ( *a1 == 34 )
  {
    v3 = a1 + 1;
    v8 = a1[1];
    result = 0;
    if ( v8 != 0 )
      goto LABEL_7;
    return result;
  }
LABEL_2:
  v10 = v8;
  while ( v10 != 0 )
  {
LABEL_7:
    while ( (sceNet_lib_750F705D(v8, (u16)a2) & 8) != 0 )
      v8 = *++v3;
    v12 = 0;
    v14 = v3;
    v13 = sceNet_lib_750F705D(*v3, (u16)a2);
    if ( (v13 & 4) == 0 )
      goto LABEL_11;
    while ( 1 )
    {
      a2 = sceNet_lib_750F705D(*v3, (u16)a2) & 4;
      if ( a2 == 0 )
        break;
      ++v12;
      ++v3;
    }
    result = 0;
    if ( v12 >= 6 )
      return result;
    sceKernelMemcpy(v16, v14, v12);
    v16[v12] = 0;
    v15 = sceNet_lib_1FB2FDDD(v16);
    if ( v15 > 0xFFFF )
      return 0;
    if ( v15 == v5 )
      v7 = 1;
    if ( v9 )
      *(u16 *)(2 * v6 + a3) = v15;
    v8 = *v3;
    ++v6;
    while ( 2 )
    {
      if ( v8 == 0x2C || v8 == 0 )
        goto LABEL_2;
      v10 = v8;
      if ( v8 != 34 )
      {
        ++v3;
LABEL_11:
        v8 = *v3;
        continue;
      }
      break;
    }
  }
  result = v6;
  if ( v7 == 0 )
    return 0;
  return result;
}


int http_ssl_fn_8(int a1, u32 *a2, int a3, int a4, u32 *a5)
{
  int v6;
  int v7;
  int result;
  int v9;
  u32 *v10;
  int v11;
  int v12;
  int v13; // dc
  int v14;
  int v15;
  int v16;
  int v17;
  int i;
  int v19;
  int v20;
  u32 *v21;
  int v22;
  int v23;
  int v24;
  int k;
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
  u32 *v38;
  int v39;
  int v40;
  int v41;
  int *v42;
  int m;
  int v44;
  int v45;
  int v46;
  u32 *v47;
  u32 *v48;
  int v49;
  int v50;
  int v51;
  u32 *v52;
  int v53;
  int v54;
  int v55;
  int j;
  int v57;
  int v58;
  int v59;
  u32 *v60;
  int v61;
  int v62;
  int v63;
  int v64;
  u8 v65[16];
  u8 v66[256];
  u8 v67[32];
  int v68;
  int v69;
  u8 v70;
  u8 v71[4];
  int v72;
  int v73;
  int v74;
  int v75;
  int v76;
  int v77;
  u8 v78[4];
  int v79;
  int v80;
  int v81;
  u32 *v82;
  u32 *v83;
  int v84;
  int v85;
  int v86;
  int v87;
  int v88;

  v6 = 0;
  v7 = *(u32 *)(a4 + 276);
  v79 = 10;
  v81 = a4;
  v82 = a2;
  v83 = a5;
  v77 = 0;
  v84 = 0;
  v85 = 0;
  if ( v7 == 0 )
    v7 = HTTP_DATA_U32(0x6C);
  if ( (v7 & 1) == 0 )
  {
    v85 = 1;
    *v83 = 0;
    goto LABEL_51;
  }
  result = 0;
  if ( a3 == 0 )
    return result;
  v9 = 0;
  if ( a3 > 0 )
  {
    v10 = v82;
    do
    {
      if ( sceSsl_lib_9299C6CE(*v10, v71, &v72, &v73) != 0 )
      {
        v11 = v72;
        if ( v73 > 0 )
        {
          v12 = v73 - 1;
          do
            v13 = v12-- != 0;
          while ( v13 );
          v11 = v72;
        }
        if ( v11 != 0 )
        {
          ((void (*)(void))sceSsl_lib_5E5C873A)();
          v72 = 0;
        }
      }
      v14 = sceSsl_lib_4BE9030A(*v10);
      v15 = v84;
      if ( v14 != 0 )
      {
        http_ssl_fn_18(v14);
      }
      else
      {
        *v83 = 2;
        v84 = v15 | 2;
      }
      if ( (v7 & 0x10) != 0 )
      {
        v61 = sceSsl_lib_341843AB(*v10, v71, &v72, &v73);
        v16 = v7 & 8;
        if ( v61 != 0 )
        {
          sceKernelMemset(v66, 0, 256);
          sceKernelMemcpy(v66, v72, v73);
          sceSsl_lib_5E5C873A(v72);
          v72 = 0;
          v63 = sceSsl_lib_0861D934(12);
          v62 = sceSsl_lib_0861D934(12);
          v64 = v62;
          if ( v63 == 0 || v62 == 0 )
          {
            if ( v63 != 0 )
              sceSsl_lib_5E5C873A(v63);
            if ( v64 != 0 )
              sceSsl_lib_5E5C873A(v64);
            v48 = v83;
            goto LABEL_95;
          }
          sceSsl_lib_E7BFC0EF(v66, v73, 1, v62);
          sceSsl_lib_A7BED83B(v63);
          if ( sceSsl_lib_047AB6BB(v63, v64) == -1 )
          {
            sceSsl_lib_5E5C873A(v63);
            sceSsl_lib_5E5C873A(v64);
            v84 |= 0x10u;
            *v83 = 9;
            v63 = 0;
            v64 = 0;
          }
          sceSsl_lib_5E5C873A(v63);
          sceSsl_lib_5E5C873A(v64);
          v16 = v7 & 8;
        }
      }
      else
      {
        v16 = v7 & 8;
      }
      v17 = *v10;
      if ( v16 == 0 )
        goto LABEL_22;
      if ( sceSsl_lib_05E019A4(v17, v71, &v72, &v73) != 0 )
      {
        sceKernelMemset(v66, 0, 256);
        sceKernelMemcpy(v66, v72, v73);
        sceSsl_lib_5E5C873A(v72);
        v72 = 0;
        v58 = sceSsl_lib_0861D934(12);
        v57 = sceSsl_lib_0861D934(12);
        v59 = v57;
        if ( v58 == 0 || v57 == 0 )
        {
          if ( v58 != 0 )
            sceSsl_lib_5E5C873A(v58);
          v60 = v83;
          if ( v59 != 0 )
          {
            sceSsl_lib_5E5C873A(v59);
            v60 = v83;
          }
          *v60 = 17;
          goto LABEL_44;
        }
        sceSsl_lib_E7BFC0EF(v66, v73, 1, v57);
        sceSsl_lib_A7BED83B(v58);
        if ( sceSsl_lib_047AB6BB(v58, v59) != 1 )
        {
          sceSsl_lib_5E5C873A(v58);
          sceSsl_lib_5E5C873A(v59);
          v17 = *v10;
          goto LABEL_22;
        }
        sceSsl_lib_5E5C873A(v58);
        sceSsl_lib_5E5C873A(v59);
        v84 |= 8u;
        *v83 = 10;
      }
      v17 = *v10;
LABEL_22:
      sceSsl_lib_37C7B76C(v17, v67, &v74);
      if ( v74 > 0 )
      {
        for ( i = v74 - 1; i != 0; --i )
          ;
        ++v9;
      }
      else
      {
        ++v9;
      }
      ++v10;
    }
    while ( v9 < a3 );
  }
  v19 = 0;
  if ( (v7 & 4) == 0 )
    goto LABEL_30;
  v51 = *v82;
  v75 = 0;
  v6 = sceSsl_lib_76A46EDA(v51, (void *)g_oid_subject_alt_name, 0, &v75, &v76);
  if ( v6 != 1 )
  {
LABEL_99:
    v52 = v82;
    goto LABEL_100;
  }
  v52 = v82;
  if ( v75 != 0 )
  {
    v6 = sceSsl_lib_C85F6FCE(&v68, v75, v76);
    if ( v6 == 0 )
    {
      v76 -= v70;
      for ( j = v76; j > 0; v76 = j )
      {
        v6 = sceSsl_lib_C85F6FCE(&v68, v69, j);
        if ( v6 != 0 )
          break;
        v6 = http_ssl_fn_19(v69, v68, *(u32 *)(v81 + 56));
        if ( v6 == 0 )
        {
          v85 = 1;
          break;
        }
        j = v76 - (v70 + v68);
        v69 += v68;
      }
      v20 = v7 & 0x180;
      if ( v85 != 0 )
        goto LABEL_31;
      v52 = v82;
      goto LABEL_100;
    }
    goto LABEL_99;
  }
LABEL_100:
  v53 = sceSsl_lib_12C7718F(*v52);
  if ( v53 != 0 )
  {
    v54 = http_ssl_fn_22(v53, *(u32 *)(v81 + 56));
    v55 = v85;
    v6 = v54;
    if ( v54 == 0 )
      v55 = 1;
    v85 = v55;
  }
LABEL_30:
  v20 = v7 & 0x180;
LABEL_31:
  v86 = v20;
  v21 = v82;
  while ( 1 )
  {
    v87 = 4 * v19;
    v22 = sceSsl_lib_4BE9030A(v21[v19]);
    v23 = v22;
    if ( v86 == 0 )
    {
      v24 = 0;
      if ( HTTP_BSS_U32(0x2654) > 0 )
        break;
    }
LABEL_39:
    if ( (v7 & 0x40) != 0 )
    {
      sceKernelMemset(v65, 0, 12);
      v38 = &v82[v87 / 4u];
      while ( 1 )
      {
        v6 = http_ssl_fn_23(v65, &v77, v7);
        if ( v6 < 0 )
          goto LABEL_44;
        if ( v77 == 0 )
          break;
        v39 = sceSsl_lib_12C7718F(v77);
        if ( v23 != 0 && v39 != 0 && sceSsl_lib_7D0E41F4(v39, v23) == 0 )
        {
          v40 = sceSsl_lib_B87FFE37(v77);
          if ( v40 == 0 )
            goto LABEL_94;
          if ( sceSsl_lib_0FF7BA68(*v38, v40) != 0 )
            goto LABEL_76;
        }
        sceSsl_lib_C1A2E14B(v77);
        v77 = 0;
      }
      ++v19;
    }
    else
    {
      ++v19;
    }
    if ( v19 >= a3 )
    {
      v32 = v85;
      if ( (v7 & 0x20) != 0 )
      {
        v84 |= 0x20u;
        goto LABEL_51;
      }
LABEL_52:
      v33 = v84;
      if ( (v7 & 4) != 0 && v32 != 1 )
        v33 = v84 | 4;
      v34 = v33;
      if ( v33 != 0 )
      {
        v35 = v81;
        *(u32 *)(v81 + 44) = v33;
        *(u32 *)(v35 + 40) = -2143072160;
        v6 = -1;
      }
      if ( *(u32 *)(v81 + 320) != 0 )
      {
        _sceKernelCheckThreadStack();
        v6 = (*(int (**)(int, u32 *, int, u32))(v81 + 320))(v34, v82, a3, *(u32 *)(v81 + 324));
        v37 = v81;
        if ( v6 >= 0 )
        {
          *(u32 *)(v81 + 40) = 0;
          *(u32 *)(v37 + 44) = 0;
          goto LABEL_57;
        }
      }
      else
      {
LABEL_57:
        if ( v6 >= 0 )
        {
          v36 = v81;
          v31 = v77;
          goto LABEL_59;
        }
      }
      if ( HTTP_BSS_U32(0x2654) != 0 )
        goto LABEL_44;
      v31 = v77;
      if ( (v7 & 0x40) != 0 )
        goto LABEL_45;
      v36 = v81;
LABEL_59:
      *(u32 *)(v36 + 84) = 1;
      if ( v31 != 0 )
        sceSsl_lib_C1A2E14B(v31);
      *v83 = 0;
      return 1;
    }
    v29 = sceSsl_lib_12C7718F(v82[v87 / 4u + 1]);
    if ( v23 != 0 && v29 != 0 )
    {
      v30 = sceSsl_lib_7D0E41F4(v23, v29);
      v21 = v82;
      if ( v30 == 0 )
        continue;
    }
    goto LABEL_44;
  }
  v88 = v22 != 0;
  for ( k = HTTP_BSS_U32(0x2658); ; k = HTTP_BSS_U32(0x2658) )
  {
    v26 = 4 * v24++;
    v27 = sceSsl_lib_12C7718F(*(u32 *)(v26 + k));
    if ( !v88 || v27 == 0 )
    {
      v28 = HTTP_BSS_U32(0x2654);
      goto LABEL_37;
    }
    v50 = sceSsl_lib_7D0E41F4(v27, v23);
    v28 = HTTP_BSS_U32(0x2654);
    if ( v50 == 0 )
      break;
LABEL_37:
    if ( v24 >= v28 )
      goto LABEL_39;
  }
LABEL_76:
  if ( sceKernelGetCompiledSdkVersion() > 101056767 )
  {
    v41 = v19;
    if ( v19 > 0 )
    {
      v42 = &v82[v19];
      for ( m = *v42; ; m = *v42 )
      {
        --v41;
        v44 = sceSsl_lib_B87FFE37(m);
        if ( v44 == 0 )
          break;
        v45 = *--v42;
        v46 = sceSsl_lib_0FF7BA68(v45, v44);
        v31 = v77;
        if ( v46 == 0 )
          goto LABEL_45;
        if ( v41 <= 0 )
          goto LABEL_83;
      }
LABEL_94:
      v48 = v83;
LABEL_95:
      v49 = v81;
      *v48 = 17;
      *(u32 *)(v49 + 40) = -2143072222;
LABEL_44:
      v31 = v77;
      goto LABEL_45;
    }
  }
LABEL_83:
  if ( v19 < 0 )
  {
LABEL_51:
    v32 = v85;
    goto LABEL_52;
  }
  v47 = &v82[v19];
  while ( 1 )
  {
    if ( v19 > 0 && sceSsl_lib_CFA2F8EB(*v47, v78, &v79, &v80) == 1 )
    {
      if ( v80 != 0 )
        break;
      v84 |= 2u;
    }
    --v19;
LABEL_88:
    --v47;
    if ( v19 < 0 )
    {
      v32 = v85;
      goto LABEL_52;
    }
  }
  v13 = v79 + 1 >= (unsigned int)v19--;
  if ( v13 )
    goto LABEL_88;
  v31 = v77;
LABEL_45:
  if ( v31 != 0 )
    sceSsl_lib_C1A2E14B(v31);
  result = 0;
  *(u32 *)(v81 + 84) = 1;
  return result;
}


int http_ssl_fn_9(int a1, int *a2, int *a3)
{
  int v3;
  int v7;
  int v8;
  int result;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;

  *a2 = 0;
  v3 = HTTP_BSS_U32(0x265C);
  *a3 = 0;
  v7 = 0;
  v18 = 0;
  v17 = v3;
  v8 = sceSsl_lib_92FA8B34(a1);
  result = 0;
  if ( HTTP_BSS_U32(0x265C) != 0 )
  {
    v10 = sceSsl_lib_6038DEDE(0, &v17, HTTP_BSS_U32(0x2660));
    if ( v10 == 0 )
      return 0;
    v11 = sceSsl_lib_58352068();
    v12 = v11;
    if ( v11 == 0
      || (sceSsl_lib_3454E622(v11, "%s", HTTP_BSS_U32(0x2664), 0x20000u, 0x20000u),
          (v7 = sceSsl_lib_0217E73C(sceSsl_lib_A920B39E, "PRIVATE KEY", v12, 0, 0)) == 0)
      || (v13 = sceSsl_lib_4BE9030A(v10)) == 0 )
    {
LABEL_17:
      sceSsl_lib_C1A2E14B(v10);
      if ( v7 != 0 )
        sceSsl_lib_7317C1BC(v7);
      if ( v12 == 0 )
        return 0;
      sceSsl_lib_3E040FD4(v12);
      return 0;
    }
    v14 = 0;
    if ( v8 > 0 )
    {
      v15 = 0;
      do
      {
        ++v14;
        v16 = sceSsl_lib_7BDAF864(a1, v15);
        if ( v16 == 0 )
          goto LABEL_17;
        if ( sceSsl_lib_7D0E41F4(v16, v13) == 0 )
        {
          v18 = 1;
          break;
        }
        v15 = v14;
      }
      while ( v14 < v8 );
    }
    if ( v18 == 1 )
    {
      *a2 = v10;
      *a3 = v7;
    }
    result = v18;
    if ( v12 != 0 )
    {
      sceSsl_lib_3E040FD4(v12);
      return v18;
    }
  }
  return result;
}


int http_ssl_fn_10(int a1, int a2, u32 *a3, unsigned int a4)
{
  int v6;
  int v7;
  int v9;
  int v10;
  int v11;
  unsigned int v12;
  unsigned int v13;
  u8 *v14;
  unsigned int v15;
  unsigned int v16;
  u8 *v17;
  unsigned int v18;
  int v19;
  int v20;

  v6 = 0;
  v7 = a2 != 0;
  v9 = sceNet_lib_BCBE14CF(a1, 45);
  v10 = -2143088603;
  if ( v9 == 0 )
    return v10;
  v11 = sceNet_lib_BCBE14CF(v9, 10);
  v10 = -2143088603;
  if ( v11 == 0 )
    return v10;
  v12 = sceNet_lib_BCBE14CF(v11, 45);
  v13 = v12;
  v10 = -2143088603;
  if ( v12 == 0 )
    return v10;
  v14 = (u8 *)(v11 + 1);
  if ( (unsigned int)v14 < v12 )
  {
    do
    {
      v15 = sceNet_lib_BCBE14CF(v14, 10);
      if ( v15 == 0 )
        return -2143088603;
      v16 = sceNet_lib_BCBE14CF(v14, 13);
      v17 = v14;
      v18 = v15 - (u32)v14 - 1;
      if ( v16 == 0 || v16 >= v15 )
        v18 = v15 - (u32)v14;
      v19 = 3 * (v18 >> 2);
      v14 = (u8 *)(v15 + 1);
      if ( v7 )
      {
        if ( a4 < v6 + v19 )
          return -2143088380;
        v19 = http_core_fn_1((u8 *)(a2 + v6), v17, v18);
        v20 = (unsigned int)v14 < v13;
        if ( v19 < 0 )
          return -2143088603;
      }
      else
      {
        v20 = (unsigned int)v14 < v13;
      }
      v6 += v19;
    }
    while ( v20 );
  }
  if ( a3 != NULL )
  {
    if ( !v7 )
      ++v6;
    *a3 = v6;
  }
  return 0;
}


int sceHttpsInit(int a1, int a2, u32 *a3, u32 *a4)
{
  int v8;
  int v9; // dc
  int result;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  u32 **v16;
  u32 *v17;
  int v18;
  int v19;
  unsigned int v20;
  int v21;
  int v22;
  int v23;
  int v24;

  v8 = 0;
  v9 = _sceKernelCheckThreadStack() < 1968;
  result = -2143088522;
  if ( v9 )
    return result;
  v11 = -2143088608;
  if ( HTTP_BSS_U32(0x2650) == 0 )
  {
    v11 = sceSsl_lib_9266C0D5();
    if ( v11 != -2147352262 )
    {
      HTTP_BSS_U32(0x2654) = 0;
      HTTP_BSS_U32(0x2658) = 0;
      if ( a2 == 0 || a1 <= 0 )
        goto LABEL_5;
      HTTP_BSS_U32(0x2654) = a1;
      v15 = http_opts_fn_2(4 * a1);
      HTTP_BSS_U32(0x2658) = v15;
      v11 = -2143088606;
      if ( v15 != 0 )
      {
        sceKernelMemset(v15, 0, 4 * a1);
        v23 = 0;
        if ( a1 > 0 )
        {
          while ( 1 )
          {
            v16 = (u32 **)(4 * v23 + a2);
            v17 = *v16;
            v24 = 4 * v23;
            v18 = v17[1];
            ++v23;
            v13 = sceSsl_lib_0861D934(v18 + 1);
            if ( v13 == 0 )
              break;
            sceKernelMemcpy(v13, **v16, (*v16)[1]);
            *(u8 *)(v13 + (*v16)[1]) = 0;
            if ( http_ssl_fn_10(v13, 0, &v20, 0) < 0 )
              goto LABEL_18;
            v8 = sceSsl_lib_0861D934(v20);
            if ( http_ssl_fn_10(v13, v8, &v21, v20) < 0 )
              goto LABEL_18;
            v22 = v8;
            v19 = sceSsl_lib_6038DEDE(0, &v22, v21);
            if ( v19 == 0 )
              goto LABEL_18;
            sceSsl_lib_5E5C873A(v13);
            sceSsl_lib_5E5C873A(v8);
            v8 = 0;
            v9 = v23 < a1;
            *(u32 *)(v24 + HTTP_BSS_U32(0x2658)) = v19;
            if ( !v9 )
              goto LABEL_30;
          }
          v11 = -2143088606;
LABEL_19:
          if ( v13 != 0 )
            sceSsl_lib_5E5C873A(v13);
          if ( v8 != 0 )
            sceSsl_lib_5E5C873A(v8);
          goto LABEL_15;
        }
LABEL_30:
        HTTP_BSS_U32(0x2654) = a1;
LABEL_5:
        if ( a3 == NULL )
        {
LABEL_10:
          if ( a4 == NULL )
          {
LABEL_13:
            result = 0;
            HTTP_BSS_U32(0x2650) = 1;
            return result;
          }
          v14 = sceSsl_lib_0861D934(a4[1] + 1);
          HTTP_BSS_U32(0x2664) = v14;
          v11 = -2143088606;
          if ( v14 != 0 )
          {
            sceKernelMemcpy(v14, *a4, a4[1]);
            *(u8 *)(HTTP_BSS_U32(0x2664) + a4[1]) = 0;
            goto LABEL_13;
          }
          goto LABEL_15;
        }
        v11 = -2143088606;
        v12 = sceSsl_lib_0861D934(a3[1] + 1);
        v13 = v12;
        if ( v12 != 0 )
        {
          sceKernelMemcpy(v12, *a3, a3[1]);
          *(u8 *)(v13 + a3[1]) = 0;
          if ( http_ssl_fn_10(v13, 0, &v20, 0) >= 0 )
          {
            v8 = sceSsl_lib_0861D934(v20);
            if ( http_ssl_fn_10(v13, v8, &v21, v20) >= 0 )
            {
              HTTP_BSS_U32(0x265C) = v8;
              HTTP_BSS_U32(0x2660) = v21;
              sceSsl_lib_5E5C873A(v13);
              goto LABEL_10;
            }
          }
LABEL_18:
          v11 = -2143088523;
        }
        goto LABEL_19;
      }
    }
  }
LABEL_15:
  if ( v11 != 0x80431020 && v11 != 0x8002013A )
    http_ssl_fn_11();
  return v11;
}


int sceHttp_68AB0F86(int a1, int a2, int a3)
{
  int v6; // dc
  int result;
  int v8;
  int v9;

  v6 = _sceKernelCheckThreadStack() < 1968;
  result = -2143088522;
  if ( !v6 )
  {
    v8 = -2143088608;
    if ( HTTP_BSS_U32(0x2650) != 0 )
      return v8;
    v8 = sceSsl_lib_9266C0D5();
    v9 = a1 | a2 | a3;
    if ( v8 == -2147352262 )
      return v8;
    result = 0;
    v8 = -2143088636;
    if ( v9 != 0 )
    {
      return v8;
    }
    else
    {
      HTTP_BSS_U32(0x2650) = 1;
      HTTP_BSS_U32(0x2654) = 0;
      HTTP_BSS_U32(0x2658) = 0;
    }
  }
  return result;
}


int sceHttpsLoadDefaultCert(int a1, int a2)
{
  int v2;
  int v5; // dc
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
  u32 *v18;
  int *v19;
  int v20;
  int *v21;
  int v22;
  int v23;
  int *v24;
  unsigned int v25;
  int *v26;
  int v27;
  int v28;

  v2 = 0;
  v25 = 0;
  v27 = 0;
  v5 = _sceKernelCheckThreadStack() < 1984;
  result = -2143088522;
  if ( v5 )
    return result;
  v7 = -2143088639;
  if ( http_opts_fn_23() == 1 )
  {
    v7 = sceCertLoader_DD629A24(a1, a2, 0, 0, 0, &v25);
    if ( v7 >= 0 )
    {
      v7 = -2143088606;
      v8 = sceSsl_lib_0861D934(v25);
      v9 = v8;
      if ( v8 != 0 )
      {
        v10 = sceCertLoader_DD629A24(a1, a2, &v26, v8, v25, &v27);
        v7 = v10;
        if ( v10 >= 0 )
        {
          if ( v26 == NULL || v10 <= 0 )
          {
LABEL_9:
            if ( v9 != 0 )
              sceSsl_lib_5E5C873A(v9);
            v11 = sceKernelGetCompiledSdkVersion() > 33882128;
            result = 0;
            if ( v11 )
              return v7;
            return result;
          }
          v12 = http_opts_fn_2(4 * (HTTP_BSS_U32(0x2654) + v10));
          v13 = v12;
          if ( v12 != 0 )
          {
            sceKernelMemset(v12, 0, 4 * (HTTP_BSS_U32(0x2654) + v7));
            v14 = HTTP_BSS_U32(0x2654);
            v15 = 0;
            if ( HTTP_BSS_U32(0x2654) > 0 )
            {
              v16 = HTTP_BSS_U32(0x2658);
              do
              {
                v17 = 4 * v15;
                v18 = (u32 *)(4 * v15++ + v16);
                *(u32 *)(v17 + v13) = *v18;
              }
              while ( v15 < v14 );
            }
            v19 = v26;
            v20 = 0;
            if ( v26 == NULL )
            {
LABEL_25:
              if ( HTTP_BSS_U32(0x2658) != 0 )
                http_opts_fn_3(HTTP_BSS_U32(0x2658));
              HTTP_BSS_U32(0x2658) = v13;
              HTTP_BSS_U32(0x2654) += v20;
              goto LABEL_9;
            }
            v21 = (int *)(4 * v15 + v13);
            while ( 1 )
            {
              ++v20;
              if ( http_ssl_fn_10(*v19, 0, &v25, 0) < 0 )
                break;
              v2 = http_opts_fn_2(v25);
              if ( v2 == 0 )
                goto LABEL_37;
              if ( http_ssl_fn_10(*v26, v2, &v27, v25) < 0 )
                break;
              v28 = v2;
              v22 = sceSsl_lib_6038DEDE(0, &v28, v27);
              v23 = v2;
              if ( v22 == 0 )
                break;
              v2 = 0;
              http_opts_fn_3(v23);
              v24 = v26;
              *v21++ = v22;
              v26 = (int *)v24[1];
              v19 = v26;
              if ( v26 == NULL )
                goto LABEL_25;
            }
            v7 = -2143088523;
          }
          else
          {
LABEL_37:
            v7 = -2143088606;
          }
          if ( v13 != 0 )
            http_opts_fn_3(v13);
          if ( v2 != 0 )
            http_opts_fn_3(v2);
        }
      }
      if ( v9 != 0 )
        http_opts_fn_3(v9);
      result = v7;
      if ( v7 >= 0 )
        return result;
    }
  }
  http_ssl_fn_11();
  return v7;
}


int http_ssl_fn_11()
{
  int result;
  int v1;
  int v2;
  int v3;

  result = -2143088639;
  if ( HTTP_BSS_U32(0x2650) == 1 )
  {
    if ( HTTP_BSS_U32(0x2658) != 0 )
    {
      v1 = 0;
      if ( HTTP_BSS_U32(0x2654) > 0 )
      {
        v2 = HTTP_BSS_U32(0x2658);
        do
        {
          v3 = *(u32 *)(4 * v1++ + v2);
          if ( v3 != 0 )
            sceSsl_lib_C1A2E14B(v3);
          v2 = HTTP_BSS_U32(0x2658);
        }
        while ( v1 < HTTP_BSS_U32(0x2654) );
      }
      http_opts_fn_3(HTTP_BSS_U32(0x2658));
      HTTP_BSS_U32(0x2658) = 0;
    }
    if ( HTTP_BSS_U32(0x265C) != 0 )
    {
      sceSsl_lib_5E5C873A(HTTP_BSS_U32(0x265C));
      HTTP_BSS_U32(0x265C) = 0;
      HTTP_BSS_U32(0x2660) = 0;
    }
    if ( HTTP_BSS_U32(0x2664) != 0 )
    {
      sceSsl_lib_5E5C873A(HTTP_BSS_U32(0x2664));
      HTTP_BSS_U32(0x2664) = 0;
    }
    HTTP_BSS_U32(0x2650) = 0;
    return 0;
  }
  return result;
}


int sceHttp_87F1E666(int a1, int a2)
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
  u32 v16[3];

  v4 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v4 )
  {
    v6 = -2143088639;
    if ( http_opts_fn_23() == 1 && (v6 = http_io_fn_43(a1)) >= 0 )
    {
      v8 = http_io_fn_89();
      v9 = v6 > 0x1000000;
      if ( v6 == 0x1000000 )
      {
        http_io_fn_23(a1, v16, v9);
        v13 = v16[0];
        v10 = -2143088384;
        if ( v16[0] != 0 )
        {
          v10 = 0;
          v14 = *(u32 *)(v16[0] + 276) | a2;
          --*(u32 *)(v16[0] + 8);
          *(u32 *)(v13 + 276) = v14;
        }
      }
      else if ( v6 != 0 )
      {
        v10 = -2143088384;
      }
      else
      {
        http_io_fn_31(a1, &v15, 0);
        v11 = v15;
        v10 = -2143088384;
        if ( v15 != 0 )
        {
          v10 = 0;
          v12 = *(u32 *)(v15 + 100) | a2;
          --*(u32 *)(v15 + 8);
          *(u32 *)(v11 + 100) = v12;
        }
      }
      http_io_fn_90(v8, v7, v9);
      return v10;
    }
    else
    {
      return v6;
    }
  }
  return result;
}


int sceHttp_3C478044(int a1, int a2)
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
  u32 v16[3];

  v4 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v4 )
  {
    v6 = -2143088639;
    if ( http_opts_fn_23() == 1 && (v6 = http_io_fn_43(a1)) >= 0 )
    {
      v8 = http_io_fn_89();
      v9 = v6 > 0x1000000;
      if ( v6 == 0x1000000 )
      {
        http_io_fn_23(a1, v16, v9);
        v13 = v16[0];
        v10 = -2143088384;
        if ( v16[0] != 0 )
        {
          v14 = *(u32 *)(v16[0] + 276) & ~a2;
          v10 = 0;
          --*(u32 *)(v16[0] + 8);
          *(u32 *)(v13 + 276) = v14;
        }
      }
      else if ( v6 != 0 )
      {
        v10 = -2143088384;
      }
      else
      {
        http_io_fn_31(a1, &v15, 0);
        v11 = v15;
        v10 = -2143088384;
        if ( v15 != 0 )
        {
          v12 = *(u32 *)(v15 + 100) & ~a2;
          v10 = 0;
          --*(u32 *)(v15 + 8);
          *(u32 *)(v11 + 100) = v12;
        }
      }
      http_io_fn_90(v8, v7, v9);
      return v10;
    }
    else
    {
      return v6;
    }
  }
  return result;
}


int http_ssl_fn_12(u32 *a1, int a2)
{
  u32 *v4;
  int v5;
  int v6;
  int v7;
  u32 v9[4];

  v4 = (u32 *)a1[233];
  v9[0] = 0;
  if ( v4 == NULL )
    v4 = (u32 *)a1[232];
  v5 = a1[7];
  a1[7] = 7;
  v6 = http_io_fn_87(v9, a1);
  a1[7] = v5;
  if ( v6 >= 0 )
  {
    v7 = sceNet_lib_D8722983(v9[0]);
    if ( sceNetInetSend(a2, v9[0], v7, 0) < 0 )
    {
      v4[8] = sceNetInetGetErrno();
      v6 = -2143088541;
      v4[9] = sceNetInetGetPspError();
    }
    else
    {
      v4[13] = 3;
      v6 = http_io_fn_83(a1, v4);
      if ( v6 < 0 )
      {
        v6 = -2143088540;
      }
      else if ( a1[215] == 200 )
      {
        a1[17] = 0;
      }
      else
      {
        v4[10] = -2143072156;
        v6 = -2143088523;
        if ( a1[7] != 2 )
          a1[15] = 1;
      }
    }
  }
  if ( v9[0] != 0 )
    http_opts_fn_3(v9[0]);
  return v6;
}


int http_ssl_fn_13(int a1, int a2)
{
  u32 *v2;
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
  int v16; // kr04_4
  int result;
  int v18;

  v2 = *(u32 **)(a1 + 932);
  if ( v2 == NULL )
    v2 = *(u32 **)(a1 + 928);
  v4 = 0x20000u;
  v5 = -2143088519;
  if ( HTTP_BSS_U32(0x2650) == 0 )
    goto LABEL_22;
  v6 = v2[69];
  v7 = v2[19];
  if ( v6 == 0 )
    v6 = ((v6) & ~0xFFU) | ((u8)(HTTP_DATA_U32(0x6C)));
  if ( v7 != 0 )
  {
    v18 = sceSsl_lib_A8F0AD39(v7, 0x20000u);
    result = 0;
    if ( v18 == 1 )
      return result;
    sceSsl_lib_3E3133D6(v2[19]);
    sceSsl_lib_54A7D8F3(v2[19]);
    sceSsl_lib_69F1B678(v2[19], 0);
    sceSsl_lib_B9C8CCE6(v2[19], 0, 0);
    sceSsl_lib_84833472(v2[19]);
    v2[19] = 0;
  }
  if ( v2[20] == 0 )
  {
    if ( (unsigned int)(v2[70] - 1) >= 2 )
      v8 = sceSsl_lib_B40D11EA();
    else
      v8 = sceSsl_lib_42FFA860();
    v2[20] = sceSsl_lib_FB8273FE(v8);
    v9 = v2[20];
    if ( v9 == 0 )
      goto LABEL_29;
    v10 = sceSsl_lib_B4D78E98(v9, 32, 536936447, 0);
    v5 = v10;
    if ( v10 < 0 )
    {
      if ( (HIWORD(v10) & 0xFFF) != 2 )
LABEL_21:
        v5 = -2143088523;
      goto LABEL_22;
    }
    sceSsl_lib_AEBF278B(v2[20], 5, 0);
    sceSsl_lib_529A9477(v2[20], http_ssl_fn_8, v2);
    v11 = v2[20];
    if ( (v6 & 2) != 0 )
    {
      sceSsl_lib_BF55C31C(v11, http_ssl_fn_9);
      v11 = v2[20];
    }
    sceSsl_lib_D5ABC603(v11, 1);
  }
  v12 = sceSsl_lib_EBFB0E3C(v2[20], v4);
  v2[19] = v12;
  if ( v12 == 0 || (v13 = sceSsl_lib_28B4DE33(a2, 0), v4 = v13, v13 == 0) )
  {
LABEL_29:
    v5 = -2143088523;
    v2[10] = -2143072222;
LABEL_22:
    http_ssl_fn_21(v2, v4);
    return v5;
  }
  sceSsl_lib_B9C8CCE6(v2[19], v13, v13);
  sceSsl_lib_80608663(v2[19]);
  v5 = sceSsl_lib_D53ACF30(v2[19]);
  v14 = sceSsl_lib_B3B04C58(v2[19], v5);
  v16 = v15;
  v4 = 4 * v14;
  switch ( v14 )
  {
    case 0:
    case 2:
    case 3:
    case 4:
    case 6:
      break;
    case 1:
      if ( v2[10] == 0 )
        v2[10] = -2143072159;
      goto LABEL_21;
    case 5:
      if ( v2[10] == 0 )
        v2[10] = -2143072158;
      v5 = -2143088523;
      break;
    default:
      v4 = v16;
      break;
  }
  result = 0;
  if ( v5 < 0 )
    goto LABEL_22;
  return result;
}


int http_ssl_fn_14(u32 *a1, int a2, int a3)
{
  int v4;
  int v5;
  int result;
  int v7;

  if ( a3 != 0 )
  {
    v7 = sceSsl_lib_E7C29542(a1[19]);
    switch ( sceSsl_lib_B3B04C58(a1[19], v7) )
    {
      case 0:
      case 2:
      case 3:
      case 4:
        goto LABEL_12;
      case 1:
        if ( a1[10] == 0 )
          a1[10] = -2143072159;
        v7 = -2143088523;
        goto LABEL_12;
      case 5:
        if ( a1[10] == 0 )
          a1[10] = -2143072158;
      case 6:
        a1[96] = 1;
LABEL_12:
        result = v7;
        break;
      default:
        result = v7;
        break;
    }
  }
  else
  {
    v4 = sceSsl_lib_A8F0AD39(a1[19], a2);
    v5 = 0;
    if ( v4 != 1 )
      return -2143088523;
    return v5;
  }
  return result;
}


int http_ssl_fn_15(int a1)
{
  int v2;
  int v4;

  v2 = sceSsl_lib_B7CA8717(*(u32 *)(a1 + 76));
  switch ( sceSsl_lib_B3B04C58(*(u32 *)(a1 + 76), v2) )
  {
    case 0:
    case 2:
    case 3:
    case 4:
    case 6:
      return v2;
    case 1:
      v4 = -2143072159;
      if ( *(u32 *)(a1 + 40) != 0 )
        return -2143088523;
      goto LABEL_8;
    case 5:
      if ( *(u32 *)(a1 + 40) == 0 )
      {
        v4 = -2143072158;
LABEL_8:
        *(u32 *)(a1 + 40) = v4;
      }
      return -2143088523;
    default:
      return v2;
  }
}


int sceHttpsGetSslError(int a1, u32 *a2, u32 *a3)
{
  int v6; // dc
  int result;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  s64 v13;
  u32 v14[4];

  v6 = _sceKernelCheckThreadStack() < 2016;
  result = -2143088522;
  if ( !v6 )
  {
    v8 = v14[0];
    if ( HTTP_BSS_U32(0x2650) == 1 )
    {
      v13 = ((u64)(v13) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(a2 == NULL) << 32);
      v13 = ((u64)(v13) & 0xFFFFFFFF00000000ULL) | (u32)(a3 == NULL);
      if ( v13 != 0 )
      {
        v8 = v14[0];
        v9 = -2143088130;
      }
      else
      {
        v9 = http_io_fn_12(a1, v14);
        v8 = v14[0];
        if ( v9 >= 0 )
        {
          *a2 = *(u32 *)(v14[0] + 52);
          *a3 = *(u32 *)(v8 + 56);
        }
      }
    }
    else
    {
      v9 = -2143088639;
    }
    if ( v8 != 0 )
    {
      v10 = http_io_fn_89();
      --*(u32 *)(v14[0] + 32);
      http_io_fn_90(v10, v11, v12);
    }
    return v9;
  }
  return result;
}


int sceHttpsSetSslCallback(int a1, int a2, int a3)
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
  u32 v18[3];

  v6 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v6 )
  {
    v8 = -2143088639;
    if ( http_opts_fn_23() == 1 && (v8 = http_io_fn_43(a1)) >= 0 )
    {
      v10 = http_io_fn_89();
      v11 = v8 > 0x1000000;
      if ( v8 == 0x1000000 )
      {
        http_io_fn_23(a1, v18, v11);
        v15 = v18[0];
        v12 = -2143088384;
        if ( v18[0] != 0 )
        {
          v16 = *(u32 *)(v18[0] + 8);
          v12 = 0;
          *(u32 *)(v18[0] + 320) = a2;
          *(u32 *)(v15 + 8) = v16 - 1;
          *(u32 *)(v15 + 324) = a3;
        }
      }
      else if ( v8 != 0 )
      {
        v12 = -2143088384;
      }
      else
      {
        http_io_fn_31(a1, &v17, 0);
        v13 = v17;
        v12 = -2143088384;
        if ( v17 != 0 )
        {
          v14 = *(u32 *)(v17 + 8);
          v12 = 0;
          *(u32 *)(v17 + 144) = a2;
          *(u32 *)(v13 + 8) = v14 - 1;
          *(u32 *)(v13 + 148) = a3;
        }
      }
      http_io_fn_90(v10, v9, v11);
      return v12;
    }
    else
    {
      return v8;
    }
  }
  return result;
}


int sceHttpsGetCaList(u32 *a1, int *a2)
{
  int v4; // dc
  int result;
  int v6;

  v4 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v4 )
  {
    result = 0;
    if ( HTTP_BSS_U32(0x2650) == 1 )
    {
      if ( a1 == NULL || a2 == NULL )
      {
        return -2143088130;
      }
      else
      {
        v6 = HTTP_BSS_U32(0x2654);
        *a1 = HTTP_BSS_U32(0x2658);
        *a2 = v6;
      }
    }
    else
    {
      return -2143088639;
    }
  }
  return result;
}


int http_ssl_fn_16()
{
  return HTTP_BSS_U32(0x2650);
}


int sceHttpsEnd()
{
  int v0; // dc
  int result;

  v0 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v0 )
    return http_ssl_fn_11();
  return result;
}


int sceHttpsEnableOption(int a1)
{
  int v2; // dc
  int result;

  v2 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v2 )
  {
    HTTP_DATA_U32(0x6C) |= a1;
    return 0;
  }
  return result;
}


int sceHttpsDisableOption(int a1)
{
  int v1;
  int v2; // dc
  int result;

  v1 = ~a1;
  v2 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v2 )
  {
    HTTP_DATA_U32(0x6C) &= v1;
    return 0;
  }
  return result;
}


int http_ssl_fn_17(int a1, int a2)
{
  return http_ssl_fn_21(a1, a2);
}


int http_ssl_fn_18(int a1)
{
  int result;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  char v9[4];
  char v10[4];
  int v11[4];

  v3 = 0;
  result = sceSsl_lib_4C75B8E9();
  v4 = result;
  if ( result > 0 )
  {
    v5 = 0;
    do
    {
      v6 = sceSsl_lib_5040E836(a1, v5);
      result = sceSsl_lib_8D484361(v6, 0, &v7, &v8, v9, v10, v11);
      if ( result != 0 )
      {
        sceSsl_lib_2B7BD166(v7, v8, 0);
        result = v11[0];
        if ( v11[0] > 0 )
        {
          for ( result = v11[0] - 1; result != 0; --result )
            ;
          ++v3;
        }
        else
        {
          ++v3;
        }
      }
      else
      {
        ++v3;
      }
      v5 = v3;
    }
    while ( v3 < v4 );
  }
  return result;
}


int http_ssl_fn_19(int a1, unsigned int a2, int a3)
{
  int v5;
  unsigned int v6;
  unsigned int v7;
  unsigned int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  unsigned int v13;
  int result;
  int v15;
  unsigned int v16;
  int v17;
  int v18;
  unsigned int v19;
  int v20;
  int v21;
  unsigned int v22;
  int v23; // dc

  v5 = a3;
  v6 = sceNet_lib_D8722983(a3);
  v7 = (__PAIR64__(a2, *(char *)(a1 + a2 - 1) ^ 0x2Eu) - 1) >> 32;
  v8 = 0;
  if ( *(u8 *)(v5 + v6 - 1) == 46 )
    --v6;
  v9 = a1;
  while ( 1 )
  {
    v10 = http_ssl_fn_20(v9, 42, v7 - v8);
    v11 = v10;
    if ( v10 == 0 )
      break;
    v12 = v10 - v9;
    v13 = v8 + v10 - v9;
    if ( v10 != v9 )
    {
      result = sceNet_lib_B9085A96(v9, v5, v10 - v9);
      v5 += v12;
      if ( result != 0 )
        return result;
      v6 -= v12;
      v13 = v8 + v12;
    }
    v8 = v13 + 1;
    v15 = a1 + v13 + 1;
    v16 = v7 - (v13 + 1);
    v17 = http_ssl_fn_20(v15, 42, v16);
    v18 = v17 - v11;
    if ( v17 == 0 )
    {
      result = -1;
      if ( v6 < v16 )
        return result;
      v21 = http_ssl_fn_20(v5, 46, v6);
      v22 = v6 - v16;
      if ( v21 != 0 )
      {
        v23 = v21 - v5 < v22;
        result = -1;
        if ( v23 )
          return result;
      }
      v6 -= v22;
      v5 += v22;
LABEL_15:
      v20 = v7 < v8;
      goto LABEL_16;
    }
    v19 = v18 - 1;
    v20 = v7 < v8;
    if ( v18 != 1 )
    {
      result = -1;
      if ( v6 < v19 )
        return result;
      while ( 1 )
      {
        result = sceNet_lib_B9085A96(v15, v5, v19);
        if ( result == 0 )
          break;
        --v6;
        ++v5;
        if ( v6 < v19 )
          goto LABEL_14;
      }
      v6 -= v19;
      v8 += v19;
      v5 += v19;
LABEL_14:
      if ( result != 0 )
        return result;
      goto LABEL_15;
    }
LABEL_16:
    v9 = a1 + v8;
    if ( v20 )
      return -1;
  }
  result = -1;
  if ( v7 - v8 == v6 )
    return sceNet_lib_B9085A96(v9, v5, v6);
  return result;
}


char *http_ssl_fn_20(char *a1, int a2, unsigned int a3)
{
  int v3;
  char *result;
  unsigned int v5;
  int v6;

  v3 = a2;
  result = NULL;
  if ( a1 != NULL )
  {
    v5 = 0;
    if ( a3 != 0 )
    {
      for ( result = a1; ; result = &a1[v5] )
      {
        v6 = *result;
        ++v5;
        if ( v6 == v3 )
          break;
        result = NULL;
        if ( v6 == 0 )
          break;
        if ( v5 >= a3 )
          return NULL;
      }
    }
  }
  return result;
}


int http_ssl_fn_21(u32 *a1)
{
  int v2;
  int result;

  v2 = a1[19];
  if ( v2 != 0 )
  {
    if ( a1[21] == 1 )
      sceSsl_lib_3E3133D6(v2);
    sceSsl_lib_54A7D8F3(a1[19]);
    sceSsl_lib_69F1B678(a1[19], 0);
    sceSsl_lib_B9C8CCE6(a1[19], 0, 0);
    result = sceSsl_lib_84833472(a1[19]);
    a1[19] = 0;
    a1[21] = 0;
  }
  if ( a1[20] != 0 )
  {
    result = sceSsl_lib_588F2FE8();
    a1[20] = 0;
  }
  return result;
}


int http_ssl_fn_22(int a1, int a2)
{
  int v3;
  int v5;
  int v6;
  int v7;
  int v8;
  int result;
  char *v10;
  int v11;
  int v13;
  int v14;
  char v15[4];
  int v16;
  unsigned int v17[4];

  v3 = 0;
  v5 = 0;
  v6 = sceSsl_lib_4C75B8E9();
  if ( v6 > 0 )
  {
    v7 = 0;
    do
    {
      v8 = sceSsl_lib_5040E836(a1, v7);
      if ( sceSsl_lib_8D484361(v8, 0, &v13, &v14, v15, &v16, v17) != 0 )
      {
        v10 = (char *)sceSsl_lib_2B7BD166(v13, v14, 0);
        if ( v10 == NULL )
          v10 = "<UNKNOWN>";
        if ( sceNet_lib_A0F16ABD(v10, "commonName") == 0 && http_ssl_fn_19(v16, v17[0], a2) == 0 )
          v3 = 1;
        if ( (int)v17[0] > 0 )
        {
          v11 = v17[0] - 1;
          while ( v11-- != 0 )
            ;
          ++v5;
        }
        else
        {
          ++v5;
        }
      }
      else
      {
        ++v5;
      }
      v7 = v5;
    }
    while ( v5 < v6 );
  }
  result = -1;
  if ( v3 == 1 )
    return 0;
  return result;
}


int http_ssl_fn_23(u32 *a1, int *a2, int a3)
{
  int v4;
  int v6;
  int v7;
  int i;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int result;
  int v15;
  int v16;
  unsigned int v17;
  int *v18;
  int v19;
  int v20;

  v17 = 0;
  v19 = 0;
  v4 = 0;
  v6 = a3 & 0x100;
  v7 = 0;
  for ( i = *a1; ; i = *a1 )
  {
    if ( i == 0 || a1[2] == 1 )
    {
      *a1 = v6 != 0 ? 3 : i + 1;
      a1[1] = 1;
    }
    else
    {
      a1[1] *= 2;
    }
    v9 = sceCertLoader_DD629A24(*a1, a1[1], 0, 0, 0, &v17);
    v10 = v9;
    if ( v9 == -2147483611 )
    {
      v10 = 0;
      if ( a1[2] == 1 )
      {
        *a2 = 0;
        return v10;
      }
      if ( v6 != 0 )
      {
        *a2 = 0;
        return v10;
      }
      a1[2] = 1;
    }
    else
    {
      v7 = 1;
      if ( v9 < 0 )
        return v10;
      a1[2] = 0;
    }
    if ( v7 != 0 )
      break;
  }
  v10 = -2143088606;
  v11 = sceSsl_lib_0861D934(v17 + 8000);
  v12 = v11;
  if ( v11 == 0 )
    goto LABEL_14;
  v13 = sceCertLoader_DD629A24(*a1, a1[1], &v18, v11, v17, &v19);
  v10 = v13;
  if ( v13 < 0 )
    goto LABEL_14;
  if ( v13 == 1 && http_ssl_fn_10(*v18, 0, &v17, 0) >= 0 )
  {
    v10 = -2143088606;
    v15 = sceSsl_lib_0861D934(v17);
    v4 = v15;
    if ( v15 == 0 )
      goto LABEL_14;
    if ( http_ssl_fn_10(*v18, v15, &v19, v17) >= 0 )
    {
      sceSsl_lib_5E5C873A(v12);
      v20 = v4;
      v16 = sceSsl_lib_6038DEDE(0, &v20, v19);
      *a2 = v16;
      v12 = 0;
      v10 = -2143088523;
      if ( v16 != 0 )
      {
        v10 = 0;
LABEL_14:
        if ( v12 != 0 )
          sceSsl_lib_5E5C873A(v12);
      }
      result = v10;
      if ( v4 == 0 )
        return result;
      sceSsl_lib_5E5C873A(v4);
      return v10;
    }
  }
  v10 = -2143088523;
  goto LABEL_14;
}


int http_ssl_fn_24(int a1)
{
  int v2;
  int v3;
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
  int *v16;
  int v17;
  int v18;
  int v19;
  int v20; // dc
  int v21;
  int v22;
  s64 v23;
  int v24[2];
  int v25;
  int v26;
  int v27[8];
  int v28;
  int v29;
  u32 v30[2];

  v2 = 0;
  v3 = 0;
  v4 = *(u32 *)(a1 + 60);
  v29 = 0;
  v30[0] = 0;
  if ( v4 == 3 )
    return 0;
  if ( *(u32 *)(a1 + 224) == 1 )
  {
    if ( *(u32 *)(a1 + 860) == 401 )
    {
      v7 = *(u32 *)(a1 + 104);
      goto LABEL_9;
    }
    v5 = *(u16 *)(a1 + 310);
  }
  else
  {
    v5 = *(u16 *)(a1 + 310);
  }
  if ( v5 != 1 )
    return 0;
  result = 0;
  if ( *(u32 *)(a1 + 860) != 407 )
    return result;
  v7 = *(u32 *)(a1 + 104);
LABEL_9:
  result = 0;
  if ( v7 == 1 || *(u32 *)(a1 + 196) < *(u32 *)(a1 + 76) )
    return result;
  v8 = http_core_fn_8((int)v24, *(u32 *)(a1 + 124), *(u32 *)(a1 + 68), *(u32 *)(a1 + 860));
  if ( v8 == -2143088606 )
    return v8;
  result = 0;
  if ( v8 < 0 )
    return result;
  v9 = *(u32 *)(a1 + 204);
  if ( v9 != 0 )
  {
    if ( sceNet_lib_D8722983(v9) != 0 )
    {
      v12 = *(u32 *)(a1 + 204);
LABEL_21:
      if ( v12 != 0 )
      {
        if ( sceNet_lib_D8722983(v12) != 0 )
        {
          v14 = *(u32 *)(a1 + 208);
          if ( v14 != 0 )
          {
            if ( sceNet_lib_D8722983(v14) != 0 )
            {
              v17 = *(u32 *)(a1 + 204);
              v18 = *(u32 *)(a1 + 208);
              goto LABEL_40;
            }
            v13 = v25;
          }
          else
          {
            v13 = v25;
          }
        }
        else
        {
          v13 = v25;
        }
      }
      else
      {
        v13 = v25;
      }
      v8 = -2143088606;
      v15 = http_opts_fn_2(v13 + 1);
      v3 = v15;
      if ( v15 == 0 )
        goto LABEL_46;
      sceKernelMemcpy(v15, v24[1], v25);
      *(u8 *)(v3 + v25) = 0;
      v16 = http_cookie_fn_1((u32 *)a1, v24[0], v3);
      v2 = (int)v16;
      if ( v16 != NULL )
      {
        v17 = a1 + 312;
        v18 = a1 + 568;
        sceKernelMemcpy(a1 + 312, v16 + 12, 256);
        sceKernelMemcpy(a1 + 568, v2 + 304, 256);
        v28 = *(u32 *)(v2 + 560);
      }
      else
      {
        v17 = a1 + 312;
        v18 = a1 + 568;
        if ( (unsigned int)(v24[0] - 2) < 2 )
        {
          sceKernelMemset(a1 + 312, 0, 256);
          sceKernelMemset(a1 + 568, 0, 256);
        }
      }
      if ( v2 == 0 )
        goto LABEL_36;
      if ( v26 == 1 )
        *(u32 *)(v2 + 8) = 2;
      if ( *(u32 *)(v2 + 8) != 2 )
      {
LABEL_36:
        if ( *(u32 *)(a1 + 248) != 0 )
        {
          _sceKernelCheckThreadStack();
          v19 = (*(int (**)(u32, int, int, int, int, u32, int *, u32 *, int *, u32))(a1 + 248))(
                  *(u32 *)(a1 + 24),
                  v24[0],
                  v3,
                  v17,
                  v18,
                  0,
                  &v29,
                  v30,
                  &v28,
                  *(u32 *)(a1 + 252));
        }
        else
        {
          _sceKernelCheckThreadStack();
          v19 = (*(int (**)(u32, int, int, int, int, u32, int *, u32 *, int *))(a1 + 244))(
                  *(u32 *)(a1 + 24),
                  v24[0],
                  v3,
                  v17,
                  v18,
                  0,
                  &v29,
                  v30,
                  &v28);
        }
        if ( v19 < 0 )
        {
          v8 = 0;
          if ( v2 != 0 )
            http_cookie_fn_5(v2);
          goto LABEL_46;
        }
      }
      if ( v2 != 0 || (v2 = http_cookie_fn_2((u32 *)a1, v24[0], v3)) != 0 )
      {
        sceKernelMemcpy(v2 + 48, v17, 256);
        sceKernelMemcpy(v2 + 304, v18, 256);
        v20 = v24[0] >= 2u;
        *(u32 *)(v2 + 560) = v28;
        if ( v20 )
          *(u32 *)(a1 + 948) = v2;
        else
          *(u32 *)(a1 + 944) = v2;
      }
LABEL_40:
      v8 = http_ssl_fn_28(a1);
      if ( v8 >= 0 )
      {
        v21 = v24[0];
        v22 = v24[0] == 0 || v24[0] == 2;
        ++*(u32 *)(a1 + 76);
        if ( v22 )
        {
          if ( v21 != 0 )
          {
            v8 = http_io_fn_76(a1, v17, v18, 1);
            sceKernelMemset(v17, 0, 256);
            sceKernelMemset(v18, 0, 256);
          }
          else
          {
            v8 = http_io_fn_76(a1, v17, v18, 0);
          }
          if ( v8 >= 0 )
          {
            v8 = 1;
            goto LABEL_46;
          }
        }
        else
        {
          v23 = ((u64)(v23) & 0xFFFFFFFF00000000ULL) | (u32)(v21 == 1);
          v23 = ((u64)(v23) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v21 == 3) << 32);
          v8 = 1;
          if ( v23 == 0 )
            goto LABEL_46;
          v8 = http_core_fn_13(v27, v24);
          if ( v8 >= 0 )
          {
            if ( v24[0] == 1 )
            {
              v8 = http_io_fn_77(a1, v17, v18, v29, v30[0], v27, 0);
            }
            else
            {
              v8 = http_io_fn_77(a1, v17, v18, v29, v30[0], v27, 1);
              sceKernelMemset(v17, 0, 256);
              sceKernelMemset(v18, 0, 256);
            }
            if ( v8 >= 0 )
            {
              if ( v2 != 0 )
              {
                http_cookie_fn_4(v2 + 568);
                sceKernelMemcpy(v2 + 568, v27, 32);
              }
              else
              {
                http_cookie_fn_4((int)v27);
              }
              v8 = 1;
              goto LABEL_46;
            }
          }
        }
        if ( v8 != -2143088606 )
          v8 = 0;
      }
LABEL_46:
      if ( v3 == 0 )
        return v8;
      http_opts_fn_3(v3);
      return v8;
    }
    v10 = *(u32 *)(a1 + 208);
  }
  else
  {
    v10 = *(u32 *)(a1 + 208);
  }
  if ( v10 == 0 )
  {
    v11 = *(u32 *)(a1 + 244);
    goto LABEL_17;
  }
  if ( sceNet_lib_D8722983(v10) != 0 )
  {
    v12 = *(u32 *)(a1 + 204);
    goto LABEL_21;
  }
  v11 = *(u32 *)(a1 + 244);
LABEL_17:
  if ( v11 != 0 )
  {
    v12 = *(u32 *)(a1 + 204);
    goto LABEL_21;
  }
  result = 0;
  if ( *(u32 *)(a1 + 248) != 0 )
  {
    v12 = *(u32 *)(a1 + 204);
    goto LABEL_21;
  }
  return result;
}


int http_ssl_fn_25(int a1)
{
  int result;

  result = http_ssl_fn_26();
  if ( result >= 0 )
    return http_ssl_fn_27(a1);
  return result;
}


s32 http_ssl_fn_26(int a1)
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
  s32 result;
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
  int v31;
  int v32;
  int v33;
  int v34;
  int v35;
  int v36;
  int v37;
  int v38;
  int v39;
  u16 v40;
  int v41;
  int v42;
  int v43;
  int v44;
  int v45;
  int v46;
  int v47;
  int v48;
  int v49;
  char *v50;
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
  u16 v62;
  int v63;
  int v64;
  int v65;
  int v66;
  int v67;
  int v68;
  int v69;
  int v70;
  int *v71;
  int v72;
  int v73;

  v2 = 0;
  v3 = 0;
  v4 = 0;
  v5 = 0;
  v6 = 0;
  v7 = 0;
  v8 = 0;
  v9 = *(u32 *)(a1 + 60);
  v64 = 0;
  v63 = 0;
  v65 = 0;
  v66 = 0;
  v67 = 0;
  if ( v9 == 3 || *(u32 *)(a1 + 216) != 1 )
    return 0;
  v10 = *(u32 *)(a1 + 860);
  if ( (unsigned int)(v10 - 300) >= 4 && v10 != 0x131 )
  {
    result = 0;
    if ( v10 != 307 )
      return result;
    v12 = *(u32 *)(a1 + 104);
  }
  else
  {
    v12 = *(u32 *)(a1 + 104);
  }
  result = 0;
  if ( v12 == 1 || *(u32 *)(a1 + 192) < *(u32 *)(a1 + 72) )
    return result;
  if ( v10 != 303 )
    goto LABEL_13;
  if ( *(u32 *)(a1 + 28) != 2 )
  {
    *(u32 *)(a1 + 28) = 0;
LABEL_13:
    v13 = *(u32 *)(a1 + 932);
    goto LABEL_14;
  }
  v13 = *(u32 *)(a1 + 932);
LABEL_14:
  v68 = v13;
  if ( v13 == 0 )
    v68 = *(u32 *)(a1 + 928);
  v14 = *(u32 *)(a1 + 904);
  v69 = -2143088540;
  if ( v14 == 0 )
    goto LABEL_80;
  v15 = *(u32 *)(a1 + 908) + 1;
  v69 = -2143088606;
  v16 = http_opts_fn_2(v15);
  v70 = v16;
  if ( v16 == 0 )
    goto LABEL_62;
  sceKernelMemcpy(v16, *(u32 *)(a1 + 904), *(u32 *)(a1 + 908));
  *(u8 *)(v70 + *(u32 *)(a1 + 908)) = 0;
  v17 = v70;
  if ( *(u32 *)(a1 + 256) != 0 )
  {
    if ( _sceKernelCheckThreadStack() - 4096 <= 0 )
    {
      v18 = (*(int (**)(u32, u32, int, int, u32))(a1 + 256))(
              *(u32 *)(a1 + 24),
              *(u32 *)(a1 + 860),
              a1 + 28,
              v70,
              *(u32 *)(a1 + 260));
    }
    else
    {
      v71 = &v55;
      v18 = (*(int (**)(u32, u32, int, int, u32))(a1 + 256))(
              *(u32 *)(a1 + 24),
              *(u32 *)(a1 + 860),
              a1 + 28,
              v70,
              *(u32 *)(a1 + 260));
    }
    v17 = v70;
    if ( v18 < 0 )
    {
      v69 = 0;
      goto LABEL_62;
    }
  }
  v69 = sceUriParse(0, v17, 0, &v63, 0);
  if ( v69 < 0 )
    goto LABEL_62;
  v69 = -2143088606;
  v19 = http_opts_fn_2(v63);
  v2 = v19;
  if ( v19 == 0 )
    goto LABEL_62;
  v69 = sceUriParse(&v56, v70, v19, 0, v63);
  if ( v69 < 0 )
    goto LABEL_62;
  if ( v60 != 0 )
  {
    v53 = sceNet_lib_D8722983(v60);
    v20 = v61;
    if ( v53 != 0 )
    {
      v69 = -2143088606;
      v63 = sceNet_lib_D8722983(v60) + 1;
      v54 = http_opts_fn_2(v63);
      v4 = v54;
      if ( v54 == 0 )
        goto LABEL_62;
      sceKernelMemcpy(v54, v60, v63);
      v20 = v61;
    }
  }
  else
  {
    v20 = v61;
  }
  if ( v20 != 0 && sceNet_lib_D8722983(v20) != 0 )
  {
    v69 = -2143088606;
    v63 = sceNet_lib_D8722983(v61) + 1;
    v52 = http_opts_fn_2(v63);
    v5 = v52;
    if ( v52 == 0 )
      goto LABEL_62;
    sceKernelMemcpy(v52, v61, v63);
  }
  v69 = http_ssl_fn_28(a1);
  if ( v69 < 0 )
  {
LABEL_62:
    v31 = v70;
    goto LABEL_63;
  }
  v21 = *(u32 *)(a1 + 860);
  ++*(u32 *)(a1 + 72);
  if ( v21 == 301 )
  {
    v6 = http_io_fn_14(a1);
    v69 = -2143088606;
    if ( v6 == 0 )
      goto LABEL_62;
    v21 = *(u32 *)(a1 + 860);
  }
  if ( v21 != 305 )
  {
    if ( v56 == 1 )
    {
      v22 = *(u32 *)(a1 + 116);
    }
    else
    {
      v23 = *(u32 *)(a1 + 112);
      if ( v23 != 0 )
      {
        http_opts_fn_3(v23);
        *(u32 *)(a1 + 112) = 0;
      }
      v22 = *(u32 *)(a1 + 116);
    }
    if ( v22 != 0 )
    {
      http_opts_fn_3(v22);
      *(u32 *)(a1 + 116) = 0;
    }
    v24 = *(u32 *)(a1 + 120);
    if ( v24 != 0 )
    {
      http_opts_fn_3(v24);
      *(u32 *)(a1 + 120) = 0;
    }
  }
  v72 = v56;
  if ( v56 == 1 )
  {
    v69 = sceUriBuild(0, &v63, 0, &v56, 10);
    if ( v69 < 0 )
      goto LABEL_62;
    v69 = -2143088606;
    v45 = http_opts_fn_2(v63);
    v67 = v45;
    if ( v45 == 0 )
      goto LABEL_62;
    v69 = sceUriBuild(v45, 0, v63, &v56, 10);
    if ( v69 < 0 )
      goto LABEL_62;
    http_opts_fn_3(v2);
    sceKernelMemset(&v56, 0, 44);
    v69 = sceUriParse(0, *(u32 *)(a1 + 112), 0, &v63, 0);
    v2 = 0;
    if ( v69 < 0 )
      goto LABEL_62;
    v69 = -2143088606;
    v46 = http_opts_fn_2(v63);
    v2 = v46;
    if ( v46 == 0 )
      goto LABEL_62;
    v69 = sceUriParse(&v56, *(u32 *)(a1 + 112), v46, 0, v63);
    if ( v69 < 0 )
      goto LABEL_62;
    v47 = *(u32 *)(a1 + 112);
    if ( v47 != 0 )
    {
      http_opts_fn_3(v47);
      *(u32 *)(a1 + 112) = 0;
    }
    v69 = sceUriBuild(0, &v63, 0, &v56, 10);
    if ( v69 < 0 )
      goto LABEL_62;
    v69 = -2143088606;
    v48 = http_opts_fn_2(v63);
    v66 = v48;
    if ( v48 == 0 )
      goto LABEL_62;
    v69 = sceUriBuild(v48, 0, v63, &v56, 10);
    if ( v69 < 0 )
      goto LABEL_62;
    v3 = http_opts_fn_1(v66, v67);
    if ( v3 == 0 )
      goto LABEL_62;
    http_opts_fn_3(v66);
    http_opts_fn_3(v67);
    v49 = *(u32 *)(a1 + 44);
    *(u32 *)(a1 + 112) = v3;
    v66 = 0;
    v67 = 0;
    v3 = 0;
    *(u32 *)(a1 + 116) = v4;
    v4 = 0;
    *(u32 *)(a1 + 120) = v5;
    v5 = 0;
    if ( v49 != 0 )
    {
      if ( *(u32 *)(v68 + 4) == v72 )
      {
        v51 = *(u16 *)(v68 + 400);
        v55 = a1;
        v50 = "https";
      }
      else
      {
        v50 = "http";
        v51 = *(u16 *)(v68 + 400);
        v55 = a1;
      }
      v30 = http_io_fn_18(0, *(u32 *)(v68 + 56), v50, 0, 0, v51, *(u32 *)(v68 + 252), 2, v55);
      v69 = v30;
LABEL_51:
      v31 = v70;
      if ( v30 < 0 )
      {
LABEL_63:
        if ( v31 != 0 )
          http_opts_fn_3(v31);
        if ( v2 != 0 )
          http_opts_fn_3(v2);
        if ( v3 != 0 )
          http_opts_fn_3(v3);
        if ( v4 != 0 )
          http_opts_fn_3(v4);
        v33 = v65;
        if ( v5 != 0 )
        {
          http_opts_fn_3(v5);
          v33 = v65;
        }
        if ( v33 != 0 )
          http_opts_fn_3(v33);
        if ( v66 != 0 )
          http_opts_fn_3(v66);
        if ( v67 == 0 )
        {
          v34 = v64;
LABEL_81:
          if ( v34 != 0 )
            http_opts_fn_3(v34);
          if ( v6 != 0 )
            http_opts_fn_3(v6);
          if ( v7 != 0 )
            http_opts_fn_3(v7);
          if ( v8 != 0 )
            http_opts_fn_3(v8);
          return v69;
        }
        http_opts_fn_3(v67);
LABEL_80:
        v34 = v64;
        goto LABEL_81;
      }
    }
  }
  else
  {
    if ( sceNet_lib_A0F16ABD(v58, *(u32 *)(v68 + 56)) != 0 )
    {
      v25 = *(u32 *)(a1 + 28);
    }
    else if ( *(u32 *)(a1 + 44) != 0 )
    {
      v25 = *(u32 *)(a1 + 28);
    }
    else
    {
      v43 = http_io_fn_37(v57);
      if ( v43 == *(u32 *)(v68 + 4) )
      {
        if ( *(u16 *)(v68 + 400) == v62 )
        {
          v69 = -2143088606;
          v63 = sceNet_lib_D8722983(v59) + 1;
          v44 = http_opts_fn_2(v63);
          v3 = v44;
          if ( v44 == 0 )
            goto LABEL_62;
          sceNet_lib_80C9F02A(v44, v59);
          http_opts_fn_3(v2);
          v2 = 0;
          *(u32 *)(a1 + 112) = v3;
          v3 = 0;
          *(u32 *)(a1 + 116) = v4;
          v4 = 0;
          *(u32 *)(a1 + 120) = v5;
          v5 = 0;
          goto LABEL_52;
        }
        v25 = *(u32 *)(a1 + 28);
      }
      else
      {
        v25 = *(u32 *)(a1 + 28);
      }
    }
    v26 = *(u32 *)(a1 + 860);
    if ( v25 == 1 && v26 != 305 && v26 != 307 && *(u32 *)(v68 + 4) == 1 )
    {
      v42 = http_io_fn_37(v57);
      v26 = *(u32 *)(a1 + 860);
      if ( v42 == 0 )
        *(u32 *)(a1 + 28) = 0;
    }
    if ( v26 != 305 )
    {
      v69 = -2143088606;
      v63 = sceNet_lib_D8722983(v59) + 1;
      v27 = http_opts_fn_2(v63);
      v3 = v27;
      if ( v27 == 0 )
        goto LABEL_62;
      sceNet_lib_80C9F02A(v27, v59);
      *(u32 *)(a1 + 112) = v3;
      v3 = 0;
      *(u32 *)(a1 + 116) = v4;
      v4 = 0;
      *(u32 *)(a1 + 120) = v5;
      v5 = 0;
      v28 = http_io_fn_37(v57);
      v69 = http_io_fn_86(&v64, v28, v58, v62);
      if ( v69 < 0 )
        goto LABEL_62;
      http_opts_fn_7(*(u32 *)(a1 + 24), "Host", v64, 0);
      http_opts_fn_3(v64);
      v29 = *(u32 *)(a1 + 212);
      v64 = 0;
      v30 = http_io_fn_19(0, v70, v29, 1, a1);
      v69 = v30;
      goto LABEL_51;
    }
    v69 = -2143088606;
    v36 = sceNet_lib_D8722983(v58);
    v37 = http_opts_fn_2(v36 + 1);
    v65 = v37;
    if ( v37 == 0 )
      goto LABEL_62;
    v73 = -2143092736;
    sceNet_lib_80C9F02A(v37, v58);
    v38 = http_io_fn_14(a1);
    v8 = v38;
    v69 = v73 | 0x1022;
    if ( v38 == 0 )
      goto LABEL_62;
    v69 = http_io_fn_19(0, v38, *(u32 *)(a1 + 212), 1, a1);
    if ( v69 < 0 )
      goto LABEL_62;
    http_opts_fn_3(v8);
    v39 = *(u32 *)(a1 + 932);
    v40 = v62;
    v41 = v65;
    *(u32 *)(v39 + 192) = 1;
    v8 = 0;
    *(u16 *)(v39 + 348) = v40;
    *(u16 *)(v39 + 272) = 0;
    *(u32 *)(v39 + 240) = v41;
    v65 = 0;
  }
LABEL_52:
  if ( *(u32 *)(a1 + 860) == 301 )
  {
    v35 = http_io_fn_14(a1);
    v7 = v35;
    v69 = -2143088606;
    if ( v35 == 0 )
      goto LABEL_62;
    http_opts_fn_14(v6, *(u32 *)(a1 + 28), 0, v35);
  }
  v32 = v70;
  if ( v2 != 0 )
  {
    http_opts_fn_3(v2);
    v2 = 0;
    v32 = v70;
  }
  if ( v32 != 0 )
  {
    http_opts_fn_3(v32);
    v70 = 0;
  }
  if ( v6 != 0 )
  {
    http_opts_fn_3(v6);
    v6 = 0;
  }
  if ( v7 != 0 )
  {
    http_opts_fn_3(v7);
    v7 = 0;
  }
  result = http_core_fn_5((int *)a1);
  v69 = result;
  if ( result < 0 )
    goto LABEL_62;
  return result;
}


int http_ssl_fn_27(int *a1)
{
  int v1;
  int result;

  v1 = a1[233];
  if ( v1 == 0 )
    v1 = a1[232];
  result = http_ssl_fn_24((int)a1);
  if ( result > 0 )
  {
    if ( a1[11] == 0 )
      return http_core_fn_5(a1);
    result = *(u32 *)(v1 + 4) == 1
           ? http_io_fn_18(
               0,
               *(u32 *)(v1 + 56),
               "https",
               0,
               0,
               *(u16 *)(v1 + 400),
               *(u32 *)(v1 + 252),
               2,
               a1)
           : http_io_fn_18(
               0,
               *(u32 *)(v1 + 56),
               "http",
               0,
               0,
               *(u16 *)(v1 + 400),
               *(u32 *)(v1 + 252),
               2,
               a1);
    if ( result >= 0 )
      return http_core_fn_5(a1);
  }
  return result;
}


int http_ssl_fn_28(u32 *a1)
{
  u32 *v2;
  int v3;

  v2 = a1 + 72;
  v3 = http_io_fn_38();
  if ( v3 >= 0 )
  {
    a1[2] = 0;
    a1[3] = 0;
    a1[4] = 0;
    a1[5] = 0;
    a1[15] = 0;
    a1[22] = 0;
    http_opts_fn_8(v2, "Proxy-Authorization");
    http_opts_fn_8(v2, "Authorization");
    http_opts_fn_8(v2, "Cookie");
    if ( a1[234] != 0 )
    {
      http_opts_fn_8(v2, "If-Modified-Since");
      http_io_fn_65(a1[234]);
      a1[234] = 0;
    }
  }
  return v3;
}


