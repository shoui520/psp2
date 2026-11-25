#include "libhttp_internal.h"

u8 g_http_data[140] __attribute__((aligned(4))) = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x10, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x2C, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3D, 0x00, 0x00, 0x00,
    0x68, 0x74, 0x74, 0x70, 0x00, 0x00, 0x00, 0x00, 0x68, 0x74, 0x74, 0x70, 0x73, 0x00, 0x00, 0x00,
    0x48, 0x6F, 0x73, 0x74, 0x00, 0x00, 0x00, 0x00, 0x2F, 0x00, 0x00, 0x00,
};
u8 g_http_bss[0x3600] __attribute__((aligned(8)));

const u8 g_oid_subject_alt_name[4] __attribute__((aligned(4))) = {
    0x55, 0x1D, 0x11, 0x00,
};
const u8 g_b64_decode_tab[256] __attribute__((aligned(4))) = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xFF, 0xFF, 0x3F,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF,
    0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};
const u8 g_clz_tab[256] __attribute__((aligned(4))) = {
    0x00, 0x01, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
};
const u8 g_md5_empty_digest[16] __attribute__((aligned(4))) = {
    0xD4, 0x1D, 0x8C, 0xD9, 0x8F, 0x00, 0xB2, 0x04, 0xE9, 0x80, 0x09, 0x98, 0xEC, 0xF8, 0x42, 0x7E,
};

int http_core_fn_1(u8 *a1, u8 *a2, int a3)
{
  int result;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10; // dc
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;

  result = 0;
  if ( a3 != 0 )
  {
    while ( 1 )
    {
      v4 = a2[2];
      v5 = a2[3];
      if ( v4 == 61 )
        --result;
      if ( v5 == 61 )
        --result;
      v6 = *((char *)g_b64_decode_tab + *a2);
      v7 = *((char *)g_b64_decode_tab + a2[1]);
      v8 = *((char *)g_b64_decode_tab + v4);
      v9 = *((char *)g_b64_decode_tab + v5);
      v10 = v6 < 0;
      v11 = v6 << 18;
      if ( v10 )
        return -1;
      v10 = v7 < 0;
      v12 = v7 << 12;
      if ( v10 )
        return -1;
      v10 = v8 < 0;
      v13 = v8 << 6;
      if ( v10 )
        return -1;
      v14 = v11 | v12;
      if ( v9 < 0 )
        return -1;
      v15 = v14 | v13 | v9;
      *a1 = BYTE2(v15);
      a2 += 4;
      a1[1] = BYTE1(v15);
      a3 -= 4;
      a1[2] = v15;
      result += 3;
      a1 += 3;
      if ( a3 <= 0 )
        goto LABEL_11;
    }
  }
  else
  {
LABEL_11:
    *a1 = 0;
  }
  return result;
}


int http_core_fn_2(char *a1, u8 *a2, unsigned int a3)
{
  int result;
  int v4;
  int v5;
  unsigned int v6;
  int v7;
  unsigned int v8;
  unsigned int v9;
  char v10;
  char v11;
  char v12;
  char v13;

  result = 0;
  if ( a3 != 0 )
  {
    do
    {
      v4 = a2[1];
      v5 = a2[2];
      if ( a3 < 2 )
        v4 = 0;
      if ( a3 < 3 )
        v5 = 0;
      v6 = (*a2 << 16) | v5 | (v4 << 8);
      v7 = v5 & 0x3F;
      v8 = v6 >> 6;
      v9 = v6 >> 12;
      v10 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[v6 >> 18];
      v11 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[v9 & 0x3F];
      v12 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[v8 & 0x3F];
      v13 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[v7];
      if ( a3 < 2 )
        v12 = 61;
      if ( a3 < 3 )
        v13 = 61;
      *a1 = v10;
      a1[1] = v11;
      a2 += 3;
      a1[2] = v12;
      a3 -= 3;
      a1[3] = v13;
      result += 4;
      a1 += 4;
    }
    while ( (int)a3 > 0 );
  }
  *a1 = 0;
  return result;
}


int http_core_fn_3(unsigned int a1, unsigned int a2, unsigned int a3, unsigned int a4)
{
  unsigned int v5;
  unsigned int v6;
  int v8;
  int v9;
  char v10;
  unsigned int v11;
  int v12;
  unsigned int v13;
  unsigned int v14;
  unsigned int v15;
  unsigned int v16;
  unsigned int v17;
  unsigned int v18;
  int v19;
  int v21;
  int v22;
  char v23;
  char v24;
  unsigned int v25;
  unsigned int v26;
  int v27;
  int v28;
  unsigned int v29;
  unsigned int v30;
  unsigned int v31;
  unsigned int v32;
  unsigned int v33;
  unsigned int v34;
  int v35;
  int v36;
  unsigned int v37;
  unsigned int v38;
  unsigned int v39;
  unsigned int v40;
  unsigned int v41;
  unsigned int v42;
  int v43;
  int v44;
  char v45;
  char v46;
  unsigned int v47;
  unsigned int v48;
  int v49;
  int v50;
  unsigned int v51;
  unsigned int v52;
  unsigned int v53;
  int v54;
  unsigned int v55;
  unsigned int v56;
  unsigned int v57;
  int v58;
  unsigned int v59;
  unsigned int v60;
  int v61;
  unsigned int v62;
  unsigned int v63;

  v5 = a2;
  v6 = a3;
  if ( a4 == 0 )
  {
    if ( a2 < a3 )
    {
      v8 = 8;
      if ( a3 < 0x100 )
        v8 = 0;
      if ( a3 > 0xFFFF )
      {
        v8 = 16;
        if ( a3 > 0xFFFFFF )
          v8 = 24;
      }
      v9 = *((u8 *)g_clz_tab + (a3 >> v8)) + v8;
      v10 = 32 - v9;
      if ( v9 != 32 )
      {
        v5 = (v5 << (32 - v9)) | (a1 >> v9);
        a1 <<= v10;
        v6 = a3 << v10;
      }
      v11 = HIWORD(v6);
      if ( HIWORD(v6) == 0 )
        _break(0, 7u);
      v12 = v5 / HIWORD(v6);
      v13 = ((v5 % HIWORD(v6)) << 16) | HIWORD(a1);
      v14 = v12 * (u16)v6;
      if ( v13 < v14 )
      {
        v13 += v6;
        --v12;
        if ( v13 >= v6 )
        {
          if ( v13 >= v14 )
          {
            v15 = v13 - v14;
            goto LABEL_18;
          }
          --v12;
          v13 += v6;
        }
      }
      v15 = v13 - v14;
LABEL_18:
      v16 = v15 / v11;
      v17 = ((v15 % v11) << 16) | (u16)a1;
      if ( v17 < v15 / v11 * (u16)v6 )
      {
        v18 = v17 + v6;
        --v16;
        if ( v18 >= v6 )
          v16 = (__PAIR64__(v16, v18) - v15 / v11 * (u16)v6) >> 32;
      }
      return (v12 << 16) | v16;
    }
    if ( a3 == 0 )
      _break(0, 7u);
    v21 = 8;
    if ( a3 < 0x100 )
      v21 = 0;
    if ( a3 > 0xFFFF )
    {
      v21 = 16;
      if ( a3 > 0xFFFFFF )
        v21 = 24;
    }
    v22 = *((u8 *)g_clz_tab + (a3 >> v21)) + v21;
    v23 = 32 - v22;
    v24 = *((u8 *)g_clz_tab + (a3 >> v21)) + v21;
    if ( v22 == 32 )
    {
      v25 = v5 - a3;
      v26 = HIWORD(a3);
      v27 = (u16)a3;
      goto LABEL_32;
    }
    v6 = a3 << v23;
    v26 = a3 << v23 >> 16;
    v35 = (v5 >> v24) / v26;
    v36 = (v5 >> v24) % v26;
    v27 = (u16)(a3 << v23);
    v37 = (v5 << v23) | (a1 >> v24);
    a1 <<= v23;
    v38 = (v36 << 16) | HIWORD(v37);
    v39 = v35 * v27;
    if ( v38 < v35 * v27 )
    {
      v38 += v6;
      if ( v38 >= v6 )
      {
        if ( v38 >= v39 )
        {
          v40 = v38 - v39;
          goto LABEL_48;
        }
        v38 += v6;
      }
    }
    v40 = v38 - v39;
LABEL_48:
    v41 = ((v40 % v26) << 16) | (u16)v37;
    v42 = v40 / v26 * v27;
    if ( v41 < v42 )
    {
      v41 += v6;
      if ( v41 >= v6 && v41 < v42 )
        v41 += v6;
    }
    v25 = v41 - v42;
LABEL_32:
    v28 = v25 / v26;
    v29 = ((v25 % v26) << 16) | HIWORD(a1);
    v30 = v25 / v26 * v27;
    if ( v29 < v30 )
    {
      v29 += v6;
      --v28;
      if ( v29 >= v6 )
      {
        if ( v29 >= v30 )
        {
          v31 = v29 - v30;
          goto LABEL_38;
        }
        --v28;
        v29 += v6;
      }
    }
    v31 = v29 - v30;
LABEL_38:
    v32 = v31 / v26;
    v33 = ((v31 % v26) << 16) | (u16)a1;
    if ( v33 < v31 / v26 * v27 )
    {
      v34 = v33 + v6;
      --v32;
      if ( v34 >= v6 )
        v32 = (__PAIR64__(v32, v34) - v31 / v26 * v27) >> 32;
    }
    return (v28 << 16) | v32;
  }
  v19 = 0;
  if ( a2 < a4 )
    return v19;
  v43 = 8;
  if ( a4 < 0x100 )
    v43 = 0;
  if ( a4 > 0xFFFF )
  {
    v43 = 16;
    if ( a4 > 0xFFFFFF )
      v43 = 24;
  }
  v44 = *((u8 *)g_clz_tab + (a4 >> v43)) + v43;
  v45 = 32 - v44;
  v46 = *((u8 *)g_clz_tab + (a4 >> v43)) + v43;
  if ( v44 == 32 )
    return a4 < v5 || a1 >= a3;
  v47 = (a4 << v45) | (a3 >> v46);
  v48 = HIWORD(v47);
  v49 = (v5 >> v46) / HIWORD(v47);
  v50 = (v5 >> v46) % HIWORD(v47);
  v51 = (v5 << v45) | (a1 >> v46);
  v52 = a1 << v45;
  v53 = a3 << v45;
  if ( HIWORD(v47) == 0 )
    _break(0, 7u);
  v54 = v49;
  v55 = (v50 << 16) | HIWORD(v51);
  v56 = v49 * (u16)v47;
  if ( v55 >= v56 )
    goto LABEL_68;
  v55 += v47;
  v54 = v49 - 1;
  if ( v55 < v47 )
    goto LABEL_68;
  if ( v55 < v56 )
  {
    v54 = v49 - 2;
    v55 += v47;
LABEL_68:
    v57 = v55 - v56;
    goto LABEL_69;
  }
  v57 = v55 - v56;
LABEL_69:
  v58 = v57 / v48;
  v59 = ((v57 % v48) << 16) | (u16)v51;
  v60 = v57 / v48 * (u16)v47;
  if ( v59 < v60 )
  {
    v59 += v47;
    --v58;
    if ( v59 >= v47 )
    {
      v61 = v54 << 16;
      if ( v59 >= v60 )
        goto LABEL_74;
      v59 += v47;
      --v58;
    }
  }
  v61 = v54 << 16;
LABEL_74:
  v19 = v61 | v58;
  v62 = v59 - v60;
  v63 = ((v61 | (unsigned int)v58) * (u64)v53) >> 32;
  if ( v62 >= v63 )
  {
    if ( v63 == v62 && v52 < (v61 | v58) * v53 )
      --v19;
  }
  else
  {
    --v19;
  }
  return v19;
}


s64 http_core_fn_4(s64 a1, unsigned int a2, unsigned int a3)
{
  unsigned int v4;
  unsigned int v5;
  unsigned int v6;
  int v7;
  int v8;
  char v9;
  unsigned int v10;
  unsigned int v11;
  unsigned int v12;
  unsigned int v13;
  unsigned int v14;
  unsigned int v15;
  unsigned int v16;
  s64 v17;
  int v19;
  int v20;
  char v21;
  unsigned int v22;
  unsigned int v23;
  int v24;
  unsigned int v25;
  unsigned int v26;
  unsigned int v27;
  unsigned int v28;
  unsigned int v29;
  int v30;
  int v31;
  unsigned int v32;
  unsigned int v33;
  unsigned int v34;
  unsigned int v35;
  unsigned int v36;
  unsigned int v37;
  int v38;
  int v39;
  char v40;
  char v41;
  unsigned int v42;
  unsigned int v43;
  int v44;
  int v45;
  unsigned int v46;
  unsigned int v47;
  unsigned int v48;
  int v49;
  unsigned int v50;
  unsigned int v51;
  unsigned int v52;
  int v53;
  unsigned int v54;
  unsigned int v55;
  int v56;
  int v57;
  unsigned int v58;
  unsigned int v59;
  unsigned int v60;
  unsigned int v61;
  unsigned int v62;
  u64 v63[2];

  v4 = a2;
  v5 = HIDWORD(a1);
  v6 = a1;
  if ( a3 == 0 )
  {
    if ( HIDWORD(a1) < a2 )
    {
      v7 = 8;
      if ( a2 < 0x100 )
        v7 = 0;
      if ( a2 > 0xFFFF )
      {
        v7 = 16;
        if ( a2 > 0xFFFFFF )
          v7 = 24;
      }
      v8 = *((u8 *)g_clz_tab + (a2 >> v7)) + v7;
      v9 = 32 - v8;
      v10 = HIWORD(a2);
      if ( v8 != 32 )
      {
        v5 = (v5 << (32 - v8)) | (v6 >> v8);
        v4 <<= v9;
        v6 <<= v9;
        v10 = HIWORD(v4);
      }
      v11 = ((v5 % v10) << 16) | HIWORD(v6);
      v12 = v5 / v10 * (u16)v4;
      if ( v11 < v12 )
      {
        v11 += v4;
        if ( v11 < v4 )
        {
          v13 = v11 - v12;
          goto LABEL_16;
        }
        if ( v11 < v12 )
          v11 += v4;
      }
      v13 = v11 - v12;
LABEL_16:
      v14 = ((v13 % v10) << 16) | (u16)v6;
      v15 = v13 / v10 * (u16)v4;
      if ( v14 < v15 )
      {
        v14 += v4;
        v16 = v14 - v15;
        if ( v14 < v4 )
        {
LABEL_21:
          v17 = v16 >> v9;
          goto LABEL_22;
        }
        if ( v14 < v15 )
          v14 += v4;
      }
      v16 = v14 - v15;
      goto LABEL_21;
    }
    if ( a2 == 0 )
      _break(0, 7u);
    v19 = 8;
    if ( a2 < 0x100 )
      v19 = 0;
    if ( a2 > 0xFFFF )
    {
      v19 = 16;
      if ( a2 > 0xFFFFFF )
        v19 = 24;
    }
    v20 = *((u8 *)g_clz_tab + (a2 >> v19)) + v19;
    v9 = 32 - v20;
    v21 = *((u8 *)g_clz_tab + (a2 >> v19)) + v19;
    if ( v20 == 32 )
    {
      v22 = v5 - a2;
      v23 = HIWORD(a2);
      v24 = (u16)a2;
      goto LABEL_34;
    }
    v4 = a2 << v9;
    v23 = a2 << v9 >> 16;
    v30 = (v5 >> v21) / v23;
    v31 = (v5 >> v21) % v23;
    v24 = (u16)(a2 << v9);
    v32 = (v5 << v9) | (v6 >> v21);
    v6 <<= v9;
    v33 = (v31 << 16) | HIWORD(v32);
    v34 = v30 * v24;
    if ( v33 < v30 * v24 )
    {
      v33 += v4;
      if ( v33 < v4 )
      {
        v35 = v33 - v34;
        goto LABEL_52;
      }
      if ( v33 < v34 )
        v33 += v4;
    }
    v35 = v33 - v34;
LABEL_52:
    v36 = ((v35 % v23) << 16) | (u16)v32;
    v37 = v35 / v23 * v24;
    if ( v36 < v37 )
    {
      v36 += v4;
      if ( v36 < v4 )
      {
        v22 = v36 - v37;
        goto LABEL_34;
      }
      if ( v36 < v37 )
        v36 += v4;
    }
    v22 = v36 - v37;
LABEL_34:
    v25 = ((v22 % v23) << 16) | HIWORD(v6);
    v26 = v22 / v23 * v24;
    if ( v25 < v26 )
    {
      v25 += v4;
      if ( v25 < v4 )
      {
        v27 = v25 - v26;
        goto LABEL_40;
      }
      if ( v25 < v26 )
        v25 += v4;
    }
    v27 = v25 - v26;
LABEL_40:
    v28 = ((v27 % v23) << 16) | (u16)v6;
    v29 = v27 / v23 * v24;
    if ( v28 < v29 )
    {
      v28 += v4;
      if ( v28 < v4 )
      {
        v16 = v28 - v29;
        goto LABEL_21;
      }
      if ( v28 < v29 )
        v28 += v4;
    }
    v16 = v28 - v29;
    goto LABEL_21;
  }
  if ( HIDWORD(a1) < a3 )
  {
    v63[0] = a1;
    return v63[0];
  }
  v38 = 8;
  if ( a3 < 0x100 )
    v38 = 0;
  if ( a3 > 0xFFFF )
  {
    v38 = 16;
    if ( a3 > 0xFFFFFF )
      v38 = 24;
  }
  v39 = *((u8 *)g_clz_tab + (a3 >> v38)) + v38;
  v40 = 32 - v39;
  v41 = *((u8 *)g_clz_tab + (a3 >> v38)) + v38;
  if ( v39 == 32 )
  {
    if ( a3 < v5 || v6 >= a2 )
    {
      v5 = (__PAIR64__(v5, v6) - __PAIR64__(a3, a2)) >> 32;
      v17 = ((u64)(v17) & 0xFFFFFFFF00000000ULL) | (u32)(v6 - a2);
    }
    else
    {
      v17 = ((u64)(v17) & 0xFFFFFFFF00000000ULL) | (u32)(v6);
    }
    v17 = ((u64)(v17) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v5) << 32);
LABEL_22:
    if ( v63 != NULL )
    {
LABEL_23:
      v63[0] = v17;
      return v63[0];
    }
    return v63[0];
  }
  v42 = (a3 << v40) | (a2 >> v41);
  v43 = HIWORD(v42);
  v44 = (v5 >> v41) / HIWORD(v42);
  v45 = (v5 >> v41) % HIWORD(v42);
  v46 = (v5 << v40) | (v6 >> v41);
  if ( HIWORD(v42) == 0 )
    _break(0, 7u);
  v47 = a2 << v40;
  v48 = v6 << v40;
  v49 = v44;
  v50 = (v45 << 16) | HIWORD(v46);
  v51 = v44 * (u16)v42;
  if ( v50 >= v51 )
    goto LABEL_77;
  v50 += v42;
  v49 = v44 - 1;
  if ( v50 < v42 )
    goto LABEL_77;
  if ( v50 < v51 )
  {
    v49 = v44 - 2;
    v50 += v42;
LABEL_77:
    v52 = v50 - v51;
    goto LABEL_78;
  }
  v52 = v50 - v51;
LABEL_78:
  v53 = v52 / v43;
  v54 = ((v52 % v43) << 16) | (u16)v46;
  v55 = v52 / v43 * (u16)v42;
  if ( v54 >= v55 )
    goto LABEL_82;
  v54 += v42;
  --v53;
  if ( v54 < v42 )
    goto LABEL_82;
  v56 = v49 << 16;
  if ( v54 < v55 )
  {
    --v53;
    v54 += v42;
LABEL_82:
    v56 = v49 << 16;
  }
  v57 = v56 | v53;
  v58 = v54 - v55;
  v59 = ((unsigned int)v57 * (u64)v47) >> 32;
  v60 = v57 * v47;
  if ( v58 < v59 || v59 == v58 && v48 < v60 )
  {
    v59 = (__PAIR64__(v59, v60) - __PAIR64__(v42, v57 * v47 - v47)) >> 32;
    v60 = v57 * v47 - v47;
  }
  v61 = v48 - v60;
  if ( v63 != NULL )
  {
    v62 = (__PAIR64__(v58, v48) - __PAIR64__(v59, v61)) >> 32;
    v17 = ((u64)(v17) & 0xFFFFFFFF00000000ULL) | (u32)((v62 << v41) | (v61 >> v40));
    v17 = ((u64)(v17) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v62 >> v40) << 32);
    goto LABEL_23;
  }
  return v63[0];
}


s32 http_core_fn_5(int *a1)
{
  int v1;
  s32 v3;
  int v4;
  int v5;
  int v6;
  int v7;
  s64 v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  s64 v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v21;
  u32 *v22;
  int v23;
  int v24;
  int v25;
  s64 v26;
  int v27;
  int v28;
  int PspError;
  int v30;
  int v31;
  int v32;
  int v33;
  unsigned int v34;
  int v35;
  int v36;
  int v37;
  int v38;
  int v39;
  int v40;
  int v41;
  int v42;
  s64 v43;
  u8 v44[48];
  int v45;
  u32 *v46;

  v1 = 0;
  v3 = 0;
  v4 = a1[233];
  v45 = 0;
  v46 = NULL;
  if ( v4 == 0 )
    v4 = a1[232];
  if ( *(u32 *)(v4 + 4) == 1 )
  {
    if ( http_ssl_fn_16() == 0 )
    {
      v3 = -2143088519;
      goto LABEL_44;
    }
    v5 = *(u32 *)(v4 + 52);
  }
  else
  {
    v5 = *(u32 *)(v4 + 52);
  }
  if ( v5 == 3 )
    http_io_fn_24(v4);
  v6 = http_io_fn_89();
  v7 = *(u32 *)(v4 + 52);
  v8 = ((u64)(v8) & 0xFFFFFFFF00000000ULL) | (u32)(v7 == 7);
  v8 = ((u64)(v8) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v7 == 2) << 32);
  v9 = v8 | HIDWORD(v8);
  if ( v8 == 0 )
  {
    v8 = ((u64)(v8) & 0xFFFFFFFF00000000ULL) | (u32)(1);
    if ( v7 != 5 && v7 != 1 )
    {
      v9 = 6;
      if ( *(u32 *)(v4 + 72) != 1 )
      {
        switch ( v7 )
        {
          case 0:
            v1 = 1;
            *(u32 *)(v4 + 52) = 2;
            goto LABEL_15;
          case 3:
            v1 = 1;
            goto LABEL_15;
          case 4:
            if ( *(u32 *)(v4 + 252) == 1 )
            {
              v42 = a1[7];
              v1 = 1;
              v43 = ((u64)(v43) & 0xFFFFFFFF00000000ULL) | (u32)(v42 == 1);
              v43 = ((u64)(v43) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v42 == 4) << 32);
              if ( v43 == 0 )
                v9 = 5;
              *(u32 *)(v4 + 52) = v9;
LABEL_15:
              http_io_fn_90(v6, v9, v8);
              v11 = (unsigned int)v3 >> 31;
              if ( v3 < 0 )
                goto LABEL_45;
              if ( a1[39] != 0 )
              {
                if ( a1[7] == 0 )
                {
                  if ( http_io_fn_85(a1[72], "Range") != 0 )
                  {
                    v12 = a1[26];
                    goto LABEL_20;
                  }
                  if ( http_io_fn_85(a1[72], "Content-Range") != 0 )
                  {
                    v12 = a1[26];
                    goto LABEL_20;
                  }
                  v40 = http_io_fn_48(a1);
                  a1[234] = v40;
                  if ( v40 != 0 )
                  {
                    v3 = http_io_fn_99(v44, *(u32 *)(v40 + 48), 0);
                    if ( v3 > 0 )
                      goto LABEL_44;
                    v41 = http_opts_fn_7(a1[6], "If-Modified-Since", v44, 0);
                    v3 = v41;
                    if ( v41 < 0 )
                    {
                      v11 = (unsigned int)v41 >> 31;
                      goto LABEL_45;
                    }
                  }
                }
                v12 = a1[26];
              }
              else
              {
                v12 = a1[26];
              }
LABEL_20:
              if ( v12 != 1 )
              {
                if ( *(u16 *)(v4 + 272) == 1 )
                  http_io_fn_4(v4 + 176);
                http_core_fn_22(a1, v4);
                http_core_fn_21(a1);
                if ( a1[55] != 0 )
                  http_io_fn_75(a1);
                v3 = http_io_fn_87(&v45, a1);
                if ( v3 < 0 )
                  goto LABEL_44;
              }
              v13 = http_io_fn_1(a1);
              v3 = v13;
              if ( v13 == -2143088541 )
              {
                a1[9] = *(u32 *)(v4 + 32);
                a1[10] = *(u32 *)(v4 + 36);
              }
              v11 = (unsigned int)v13 >> 31;
              if ( v13 >= 0 )
              {
                v14 = a1[15];
                if ( v14 != 0 )
                  goto LABEL_30;
                v36 = sceNet_lib_D8722983(v45);
                v37 = http_io_fn_6(v4, v45, v36, 0);
                v3 = v37;
                if ( v37 == -2143088523 )
                {
                  http_io_fn_42(v4, a1);
                  v11 = 1;
                  goto LABEL_45;
                }
                if ( v37 >= 0 )
                {
                  if ( a1[7] == 1 || a1[7] == 4 )
                  {
                    v14 = 2;
                    a1[15] = 2;
                  }
                  else
                  {
                    a1[15] = 3;
                    v14 = 3;
                  }
LABEL_30:
                  if ( v14 == 2 )
                  {
                    if ( a1[7] == 1 )
                    {
                      if ( a1[33] != 0 )
                      {
                        if ( a1[20] != 0 )
                        {
                          v26 = *((u64 *)a1 + 1);
                          if ( (unsigned int)a1[20] < (u64)(*(u64 *)a1 - v26) )
                          {
                            v34 = a1[20];
                            v35 = a1[6];
                            v26 = ((u64)(v26) & 0xFFFFFFFF00000000ULL) | (u32)(v26 + v34);
                            a1[2] = v26;
                            a1[3] = HIDWORD(v26) + ((unsigned int)v26 < v34);
                            *(u32 *)(v4 + 24) = v35;
                          }
                          else
                          {
                            v33 = a1[2];
                            v27 = *a1;
                            *((u64 *)a1 + 1) = *(u64 *)a1;
                            a1[20] = v27 - v33;
                            a1[15] = 3;
                            *(u32 *)(v4 + 24) = 0;
                          }
                          v28 = http_io_fn_6(v4, a1[33], a1[20], 0);
                          v3 = v28;
                          if ( v28 == -2143088523 )
                          {
                            http_io_fn_42(v4, a1);
                            v31 = v4;
                            a1[15] = 6;
                          }
                          else
                          {
                            if ( v28 >= 0 )
                            {
                              v14 = a1[15];
LABEL_35:
                              if ( v14 == 3 )
                                *(u32 *)(v4 + 52) = 4;
                              v18 = http_io_fn_89();
                              if ( *(u32 *)(v4 + 24) == 0 )
                              {
                                v17 = *(u32 *)(v4 + 12);
                                *(u32 *)(v4 + 12) = v17 + 1;
                              }
                              if ( *(int *)(v4 + 12) < 2 )
                              {
                                v21 = 0;
                                *(u32 *)(v4 + 20) = a1[6];
                                *(u32 *)(v4 + 16) = a1[6];
                              }
                              else
                              {
                                v21 = 1;
                                http_io_fn_12(*(u32 *)(v4 + 20), &v46);
                                v22 = v46;
                                v23 = a1[6];
                                a1[15] = 3;
                                v24 = v22[8];
                                v25 = v22[6];
                                v22[11] = v23;
                                v22[8] = v24 - 1;
                                a1[12] = v25;
                                *(u32 *)(v4 + 20) = v23;
                              }
                              http_io_fn_90(v18, v16, v17);
                              if ( ((a1[15] == 3) & (v21 ^ 1)) != 0 )
                                v3 = http_io_fn_84(a1);
                              goto LABEL_44;
                            }
                            a1[9] = sceNetInetGetErrno();
                            v3 = -2143088541;
                            PspError = sceNetInetGetPspError();
                            v30 = a1[9];
                            a1[10] = PspError;
                            v31 = v4;
                            *(u32 *)(v4 + 32) = v30;
                            v32 = a1[10];
                            a1[15] = 7;
                            *(u32 *)(v4 + 36) = v32;
                          }
                          http_io_fn_24(v31);
                          v11 = (unsigned int)v3 >> 31;
                          goto LABEL_45;
                        }
                        v15 = ((u64)(v15) & 0xFFFFFFFF00000000ULL) | (u32)(*a1);
                      }
                      else
                      {
                        v15 = ((u64)(v15) & 0xFFFFFFFF00000000ULL) | (u32)(*a1);
                      }
                    }
                    else
                    {
                      v15 = ((u64)(v15) & 0xFFFFFFFF00000000ULL) | (u32)(*a1);
                    }
                  }
                  else
                  {
                    v15 = ((u64)(v15) & 0xFFFFFFFF00000000ULL) | (u32)(*a1);
                  }
                  v15 = ((u64)(v15) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(a1[1]) << 32);
                  if ( v15 == 0 && *(u32 *)(v4 + 52) == 6 )
                  {
                    v14 = 3;
                    a1[15] = 3;
                  }
                  goto LABEL_35;
                }
                a1[9] = sceNetInetGetErrno();
                v3 = -2143088541;
                v38 = sceNetInetGetPspError();
                v39 = a1[9];
                a1[10] = v38;
                *(u32 *)(v4 + 32) = v39;
                *(u32 *)(v4 + 36) = a1[10];
LABEL_44:
                v11 = (unsigned int)v3 >> 31;
              }
LABEL_45:
              v19 = v45;
              if ( (v1 & v11) != 0 && *(u32 *)(v4 + 52) != 3 )
              {
                http_io_fn_24(v4);
                v19 = v45;
              }
              if ( v19 != 0 )
                http_opts_fn_3();
              if ( v3 == -2143088541 && a1[9] == 0xB )
                v3 = -2143088536;
              return http_min_s32(v3, 0);
            }
            break;
          case 6:
            v10 = *(u32 *)(v4 + 24);
            if ( v10 == a1[6] )
            {
              a1[26] = 1;
            }
            else if ( v10 != 0 )
            {
              v3 = -2143088607;
            }
            goto LABEL_15;
          default:
            goto LABEL_15;
        }
      }
    }
  }
  v3 = -2143088607;
  goto LABEL_15;
}


int sceHttpIsRequestInCache(int a1)
{
  int v2; // dc
  int result;
  int v4;
  int v5;
  u32 *v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 *v11;
  int v12;
  int v13;
  int v14;
  int v15;
  u32 *v16;

  v2 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( v2 )
    return result;
  v16 = NULL;
  v4 = 0;
  v5 = 0;
  if ( http_opts_fn_23() == 1 )
  {
    v10 = http_io_fn_12(a1, &v16);
    v11 = v16;
    if ( v10 >= 0 )
    {
      if ( v16[15] != 0 && v16[15] != 2 )
      {
        v6 = v16;
        goto LABEL_4;
      }
      if ( v16[54] == 1 )
      {
        v15 = http_core_fn_6(v16);
        v11 = v16;
        v5 = v15;
      }
      if ( v11[39] != 0 && v11[7] == 0 && http_io_fn_85(v11[72], "Range") == 0 && http_io_fn_85(v16[72], "Content-Range") == 0 )
      {
        if ( v5 != 0 )
        {
          v12 = http_io_fn_49(v16[39], *(u32 *)(v5 + 48), v16[7], 0);
        }
        else
        {
          v13 = http_io_fn_14(v16);
          v14 = v13;
          if ( v13 == 0 )
            goto LABEL_15;
          v12 = http_io_fn_49(v16[39], v13, v16[7], 0);
          http_opts_fn_3(v14);
        }
        if ( v12 != 0 )
        {
          v4 = 1;
          http_io_fn_64(v12);
        }
      }
LABEL_15:
      if ( v5 != 0 )
      {
        v16[18] = 0;
        http_opts_fn_16(v5);
        v6 = v16;
        goto LABEL_4;
      }
    }
  }
  v6 = v16;
LABEL_4:
  if ( v6 != NULL )
  {
    v7 = http_io_fn_89();
    v8 = v16[8];
    v16[8] = v8 - 1;
    http_io_fn_90(v7, v8, v9);
  }
  return v4;
}


int sceHttpSendRequestInCacheFirstMode(int a1, int a2, int a3)
{
  int v6; // dc
  int result;
  int v8;
  int v9;
  u32 *v10;
  int v11;
  int v12;
  int v13;
  u32 *v14;
  int v15;
  u32 *v16;
  u32 *v17;
  u32 *v18;
  int v19;
  int v20;
  u32 *v21;
  int v22;
  u32 *v23;

  v6 = _sceKernelCheckThreadStack() < 10192;
  result = -2143088522;
  if ( v6 )
    return result;
  v23 = NULL;
  v8 = 0;
  v9 = -2143088639;
  if ( http_opts_fn_23() != 1 )
    return v9;
  v9 = http_io_fn_12(a1, &v23);
  if ( v9 < 0 )
    return v9;
  v10 = v23;
  if ( v23[15] == 1 )
    v23[15] = 0;
  v11 = v10[15];
  if ( v11 != 0 && v11 != 2 )
  {
    if ( v11 == 4 )
    {
      v12 = v10[54];
      goto LABEL_13;
    }
    return -2143088538;
  }
  v12 = v10[54];
LABEL_13:
  if ( v12 == 1 )
  {
    v22 = http_core_fn_6(v10);
    v10 = v23;
    v8 = v22;
  }
  if ( v10[39] != 0 && v10[7] == 0 && http_io_fn_85(v10[72], "Range") == 0 && http_io_fn_85(v23[72], "Content-Range") == 0 )
  {
    v16 = v23;
    if ( v8 != 0 )
    {
      v16[234] = http_io_fn_49(v23[39], *(u32 *)(v8 + 48), v23[7], 0);
      v17 = v23;
    }
    else
    {
      v19 = http_io_fn_14(v23);
      v20 = v19;
      if ( v19 == 0 )
        goto LABEL_20;
      v21 = v23;
      v21[234] = http_io_fn_49(v23[39], v19, v23[7], 0);
      http_opts_fn_3(v20);
      v17 = v23;
    }
    if ( v17[234] != 0 )
    {
      v17[15] = 4;
      http_io_fn_74();
      v18 = v23;
      v9 = v23[17];
      v23[16] = 2;
      v18[21] = 0;
LABEL_20:
      if ( v8 != 0 )
        http_opts_fn_16(v8);
      if ( v23 != NULL )
      {
        v13 = http_io_fn_89();
        v14 = v23;
        v15 = v23[8] - 1;
        v23[8] = v15;
        http_io_fn_90(v13, v14, v15);
      }
      return v9;
    }
  }
  if ( v8 != 0 )
    v23[18] = 0;
  v9 = http_core_fn_7(a1, a2, a3);
  goto LABEL_20;
}


int sceHttpAbortRequest(int a1)
{
  int v2; // dc
  int result;
  int v4;
  u32 *v5;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 *v10;
  int v11;
  u32 *v12;
  int PspError;
  u32 *v14;
  u32 *v15;

  v2 = _sceKernelCheckThreadStack() < 10208;
  result = -2143088522;
  if ( v2 )
    return result;
  v15 = NULL;
  v4 = http_opts_fn_23();
  v5 = NULL;
  if ( v4 == 1 )
  {
    v6 = http_io_fn_12(a1, &v15);
    if ( v6 >= 0 )
    {
      v5 = v15;
      v10 = (u32 *)v15[233];
      if ( v10 == NULL )
        v10 = (u32 *)v15[232];
      v11 = v10[13];
      v6 = -2143088528;
      if ( v11 == 0 )
        goto LABEL_4;
      if ( v11 == 1 )
      {
        v6 = sceNetResolverStop(v10[7]);
        if ( v6 < 0 )
        {
          sceNetInetSocketAbort(v10[12]);
          v5 = v15;
          goto LABEL_4;
        }
      }
      else
      {
        v6 = sceNetInetSocketAbort(v10[12]);
        if ( v6 < 0 )
        {
          v12 = v15;
          v12[9] = sceNetInetGetErrno();
          v6 = -2143088541;
          v14 = v15;
          PspError = sceNetInetGetPspError();
          v5 = v15;
          v14[10] = PspError;
          v10[8] = v5[9];
          v10[9] = v5[10];
          goto LABEL_4;
        }
      }
    }
    v5 = v15;
  }
  else
  {
    v6 = -2143088639;
  }
LABEL_4:
  if ( v5 != NULL )
  {
    v7 = http_io_fn_89();
    --v15[8];
    http_io_fn_90(v7, v8, v9);
  }
  return v6;
}


int sceHttpReadData(int a1, int a2, unsigned int a3)
{
  int v6; // dc
  int result;
  unsigned int v8;
  int v9;
  u32 *v10;
  int v11;
  int v12;
  int v13;
  int v14;
  u32 *v15;
  int v16;
  int v17;
  int i;
  int v19;
  int v20;
  unsigned int v21;
  unsigned int v22;
  int v23;
  unsigned int v24;
  int v25;
  int v26;
  s64 v27;
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
  s64 v38;
  int v39;
  int v40;
  int v41;
  int v42;
  int v43;
  int v44;
  int v45;
  int v46;
  int v47;
  u32 v48[4];

  v48[0] = 0;
  v6 = _sceKernelCheckThreadStack() < 10176;
  result = -2143088522;
  if ( v6 )
    return result;
  v8 = 0;
  v9 = 0;
  if ( http_opts_fn_23() != 1 )
  {
    v10 = (u32 *)v48[0];
    v11 = -2143088639;
    goto LABEL_4;
  }
  v11 = http_io_fn_12(a1, v48);
  if ( v11 < 0 )
    goto LABEL_92;
  v10 = (u32 *)v48[0];
  v15 = *(u32 **)(v48[0] + 932);
  if ( v15 == NULL )
    v15 = *(u32 **)(v48[0] + 928);
  v16 = *(u32 *)(v48[0] + 60);
  v11 = -2143088539;
  if ( v16 == 0 )
    goto LABEL_4;
  v11 = -2143088541;
  if ( v16 == 7 )
    goto LABEL_4;
  v11 = -2143088532;
  if ( v16 == 8 )
    goto LABEL_4;
  if ( v16 == 3 )
  {
    v11 = -2143088530;
    if ( v15[4] != *(u32 *)(v48[0] + 24) )
      goto LABEL_4;
    v11 = http_io_fn_84(v48[0]);
    if ( v11 >= 0 )
    {
      v10 = (u32 *)v48[0];
      goto LABEL_15;
    }
LABEL_92:
    v10 = (u32 *)v48[0];
    goto LABEL_4;
  }
LABEL_15:
  v11 = -2143088529;
  if ( v10[7] == 2 )
    goto LABEL_4;
  if ( v10[15] == 1 )
    v10[15] = 4;
  if ( v10[16] == 2 )
  {
    v42 = v10[21];
    v43 = *(u32 *)(v10[234] + 48);
    v44 = *(u32 *)(v43 + 12);
    if ( v44 - v42 < a3 )
      a3 = v44 - v42;
    http_io_fn_53(a2, v43 + 32, v42, a3);
    *(u32 *)(v48[0] + 84) += a3;
    v45 = http_io_fn_89();
    --*(u32 *)(v48[0] + 32);
    http_io_fn_90(v45, v46, v47);
    return a3;
  }
  v11 = 0;
  if ( v10[15] == 5 )
    goto LABEL_4;
  if ( v10[210] == 1 )
  {
    if ( v10[208] == v10[4] )
    {
      if ( v10[209] == v10[5] )
      {
LABEL_99:
        v39 = http_io_fn_89();
        --*(u32 *)(v48[0] + 32);
        http_io_fn_90(v39, v40, v41);
        return 0;
      }
      v17 = v10[210];
    }
    else
    {
      v17 = v10[210];
    }
  }
  else
  {
    v17 = v10[210];
  }
  if ( v17 == 0 && v10[15] == 6 )
    goto LABEL_99;
  for ( i = true; ; i = v26 != 0 )
  {
    if ( !i || v8 >= a3 )
    {
      v19 = v10[235];
      goto LABEL_46;
    }
    if ( v10[15] == 5 )
    {
      v19 = v10[235];
      goto LABEL_46;
    }
    if ( v10[210] != 0 )
    {
      v21 = v10[209];
      v22 = v10[5];
      if ( v22 >= v21 )
      {
        if ( v21 != v22 )
        {
          v19 = v10[235];
          goto LABEL_46;
        }
        if ( v10[4] >= v10[208] )
        {
          v19 = v10[235];
          goto LABEL_46;
        }
        v20 = v10[210];
      }
      else
      {
        v20 = v10[210];
      }
    }
    else
    {
      v20 = v10[210];
    }
    v23 = a2 + v8;
    v24 = a3 - v8;
    if ( v20 != 1 )
      goto LABEL_39;
    v38 = *((u64 *)v10 + 104) - *((u64 *)v10 + 2);
    if ( HIDWORD(v38) != 0 )
    {
      v25 = v10[211];
    }
    else
    {
      if ( (unsigned int)v38 < v24 )
      {
        v24 = v10[208] - v10[4];
LABEL_39:
        v25 = v10[211];
        goto LABEL_40;
      }
      v25 = v10[211];
    }
LABEL_40:
    if ( v25 == 1 )
      v26 = http_opts_fn_24(v10, v23, v24, 0);
    else
      v26 = http_opts_fn_25(v10, v23, v24, 0);
    v11 = v26;
    if ( v26 < 0 )
      goto LABEL_92;
    v10 = (u32 *)v48[0];
    if ( v26 == 0 )
      break;
    v8 += v26;
    v27 = *(u64 *)(v48[0] + 16) + v26;
    *(u32 *)(v48[0] + 16) = v27;
    v10[5] = HIDWORD(v27);
  }
  v9 = 1;
  v19 = *(u32 *)(v48[0] + 940);
LABEL_46:
  if ( v19 == 0 )
  {
    v28 = v10[210];
    goto LABEL_60;
  }
  if ( v10[210] != 1 )
  {
    v29 = v10[210];
    goto LABEL_50;
  }
  if ( *(u32 *)(*(u32 *)(v19 + 48) + 12) < *(u32 *)(*(u32 *)(v19 + 48) + 20) + v8 )
  {
    v31 = v19;
    goto LABEL_88;
  }
  v29 = v10[210];
LABEL_50:
  if ( v29 == 1 )
  {
    v30 = v10[235];
    goto LABEL_55;
  }
  if ( v10[46] < *(u32 *)(*(u32 *)(v10[235] + 48) + 12) + v8 )
  {
    v31 = v10[235];
    goto LABEL_88;
  }
  v30 = v10[235];
LABEL_55:
  if ( http_io_fn_54(*(u32 *)(v30 + 48) + 32, *(u32 *)(*(u32 *)(v30 + 48) + 20), a2, v8) != v8 )
  {
    v31 = *(u32 *)(v48[0] + 940);
LABEL_88:
    http_io_fn_52(v31);
    v10 = (u32 *)v48[0];
    *(u32 *)(v48[0] + 940) = 0;
    goto LABEL_59;
  }
  v10 = (u32 *)v48[0];
  v32 = *(u32 *)(v48[0] + 940);
  *(u32 *)(*(u32 *)(v32 + 48) + 20) += v8;
  if ( v10[210] == 1 )
  {
    v28 = v10[210];
    goto LABEL_60;
  }
  *(u32 *)(*(u32 *)(v32 + 48) + 12) += v8;
LABEL_59:
  v28 = v10[210];
LABEL_60:
  if ( v28 != 1 )
  {
    v33 = v10[15];
    goto LABEL_62;
  }
  if ( v10[4] != v10[208] )
  {
    v33 = v10[15];
    goto LABEL_62;
  }
  if ( v10[5] == v10[209] )
    goto LABEL_77;
  v33 = v10[15];
LABEL_62:
  if ( v33 == 5 )
  {
LABEL_77:
    v34 = http_io_fn_89();
    v35 = v48[0];
    --v15[3];
    v15[4] = *(u32 *)(v35 + 44);
    http_io_fn_90(v34, v36, v37);
    v10 = (u32 *)v48[0];
    if ( *(u32 *)(v48[0] + 940) != 0 )
    {
      http_io_fn_51();
      v10 = (u32 *)v48[0];
    }
    if ( v10[212] == 1 )
    {
      http_io_fn_24(v15);
      v10 = (u32 *)v48[0];
    }
    v10[15] = 5;
  }
  if ( v10[210] == 1 && v10[4] == v10[208] && v10[5] == v10[209] && v15[63] == 0 || v9 == 1 )
  {
    http_io_fn_24(v15);
    v10 = (u32 *)v48[0];
  }
  if ( v10[210] == 1 )
  {
    v11 = v8;
    if ( v10[208] == v10[4] && v10[209] == v10[5] )
    {
      if ( v10[24] == 1 )
      {
        http_io_fn_24(v15);
        v10 = (u32 *)v48[0];
        goto LABEL_66;
      }
      v11 = v8;
    }
  }
  else
  {
LABEL_66:
    v11 = v8;
  }
LABEL_4:
  if ( v10 != NULL )
  {
    v12 = http_io_fn_89();
    --*(u32 *)(v48[0] + 32);
    http_io_fn_90(v12, v13, v14);
  }
  return v11;
}


int sceHttpSendRequest(int a1, int a2, int a3)
{
  int v6; // dc
  int result;

  v6 = _sceKernelCheckThreadStack() < 10224;
  result = -2143088522;
  if ( !v6 )
    return http_core_fn_7(a1, a2, a3);
  return result;
}


int sceHttpCreateTemplate(int a1, int a2, int a3)
{
  int v6; // dc
  int result;

  v6 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v6 )
    return http_io_fn_27(a1, a2, a3);
  return result;
}


int sceHttpDeleteTemplate(int a1)
{
  int v2; // dc
  int result;

  v2 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v2 )
    return http_io_fn_29(a1);
  return result;
}


int sceHttpCreateConnection(int a1, int a2, int a3, unsigned int a4, int a5)
{
  int v9;
  int v10; // dc
  int result;

  v9 = a4;
  v10 = _sceKernelCheckThreadStack() < 2000;
  result = -2143088522;
  if ( !v10 )
    return http_io_fn_18(a1, a2, a3, 0, 0, v9, a5, 0, 0);
  return result;
}


int sceHttpCreateConnectionWithURL(int a1, int a2, int a3)
{
  int v6; // dc
  int result;

  v6 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v6 )
    return http_io_fn_19(a1, a2, a3, 0, 0);
  return result;
}


int sceHttpDeleteConnection(int a1)
{
  int v2; // dc
  int result;

  v2 = _sceKernelCheckThreadStack() < 10224;
  result = -2143088522;
  if ( !v2 )
    return http_io_fn_21(a1);
  return result;
}


int sceHttpCreateRequest(int a1, int a2, int a3, int a4, int a5, int a6)
{
  int v10;
  int v12;
  int v13; // dc
  int result;

  v10 = _sceKernelCheckThreadStack();
  v12 = v10 < 2016;
  v13 = v10 < 2016;
  result = -2143088522;
  if ( !v13 )
    return http_io_fn_16(a1, a2, a3, v12, a5, a6);
  return result;
}


int sceHttpCreateRequestWithURL(int a1, int a2, int a3, int a4, int a5, int a6)
{
  int v10;
  int v12;
  int v13; // dc
  int result;

  v10 = _sceKernelCheckThreadStack();
  v12 = v10 < 2016;
  v13 = v10 < 2016;
  result = -2143088522;
  if ( !v13 )
    return http_io_fn_10(a1, a2, a3, v12, a5, a6);
  return result;
}


int sceHttpDeleteRequest(int a1)
{
  int v2; // dc
  int result;

  v2 = _sceKernelCheckThreadStack() < 10224;
  result = -2143088522;
  if ( !v2 )
    return http_io_fn_11(a1);
  return result;
}


int sceHttp_4E4A284A(int a1)
{
  int v2; // dc
  int result;

  v2 = _sceKernelCheckThreadStack() < 2032;
  result = -2143088522;
  if ( !v2 )
    return http_io_fn_28(a1);
  return result;
}


int sceHttp_A909F2AE()
{
  return -2143088130;
}


int http_core_fn_6(int a1)
{
  int v2;
  int v3;
  int v4;
  int v5;

  v2 = 0;
  v3 = http_opts_fn_13();
  if ( v3 != 0 )
  {
    while ( 1 )
    {
      v4 = *(u32 *)(a1 + 72);
      if ( *(u32 *)(a1 + 192) < (unsigned int)(v4 + 1) )
        break;
      if ( *(u32 *)(a1 + 256) != 0 )
      {
        _sceKernelCheckThreadStack();
        if ( (*(int (**)(u32, int, int, u32, u32))(a1 + 256))(
               *(u32 *)(a1 + 24),
               301,
               a1 + 28,
               *(u32 *)(v3 + 48),
               *(u32 *)(a1 + 260)) < 0 )
        {
          *(u32 *)(a1 + 216) = 0;
          http_opts_fn_16(v3);
          return v2;
        }
        v4 = *(u32 *)(a1 + 72);
      }
      *(u32 *)(a1 + 72) = v4 + 1;
      if ( v2 != 0 )
        http_opts_fn_16(v2);
      v5 = http_opts_fn_15(*(u32 *)(v3 + 48), *(u32 *)(a1 + 28), 0);
      v2 = v3;
      if ( v5 == 0 )
        return v3;
      v3 = v5;
    }
    http_opts_fn_16(v3);
    return v2;
  }
  return v3;
}


s32 http_core_fn_7(int a1, int a2, int a3)
{
  int v6;
  int v7;
  s32 v8;
  int *v9;
  int v11;
  int v12;
  int v13;
  int *v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int *v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int *v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int *v30;
  int v31;
  int v32;
  int v33;
  int v34;
  u32 v35[4];
  int v36;
  int v37;
  int v38;
  int v39;
  u16 v40;
  int *v41;
  int v42;
  int v43;

  v6 = 0;
  v7 = 0;
  v41 = NULL;
  sceKernelMemset(v35, 0, 44);
  v8 = -2143088639;
  if ( http_opts_fn_23() != 1 || (v8 = http_io_fn_12(a1, &v41)) < 0 || (v14 = v41, v8 = -2143088538, v41[15] != 0 && v41[15] != 2) )
  {
    v9 = v41;
LABEL_3:
    if ( v9 != NULL )
    {
      v11 = http_io_fn_89();
      --v41[8];
      http_io_fn_90(v11, v12, v13);
    }
    return v8;
  }
  if ( v41[7] == 1 )
  {
    v41[33] = a2;
    v14[20] = a3;
  }
  if ( v14[54] == 1 )
  {
    v17 = http_core_fn_6((int)v14);
    v7 = v17;
    v14 = v41;
    if ( v17 != 0 )
    {
      if ( http_io_fn_19(0, *(u32 *)(v17 + 48), v41[53], 1, v41) < 0 )
      {
        http_opts_fn_17(v7);
        v7 = 0;
        v41[18] = 0;
      }
      else
      {
        v8 = sceUriParse(0, *(u32 *)(v7 + 48), 0, &v42, 0);
        if ( v8 < 0 )
          goto LABEL_15;
        v8 = -2143088606;
        v18 = http_opts_fn_2(v42);
        v6 = v18;
        if ( v18 == 0 )
          goto LABEL_15;
        sceUriParse(v35, *(u32 *)(v7 + 48), v18, 0, v42);
        v19 = v38;
        if ( v37 != 0 )
        {
          v20 = v41;
          v21 = v41[28];
          if ( v21 != 0 )
          {
            http_opts_fn_3(v21);
            v20 = v41;
          }
          v8 = -2143088606;
          v22 = sceNet_lib_D8722983(v37);
          v20[28] = http_opts_fn_2(v22 + 1);
          v23 = v41[28];
          if ( v23 == 0 )
            goto LABEL_15;
          sceNet_lib_80C9F02A(v23, v37);
          v19 = v38;
        }
        v24 = v39;
        if ( v19 != 0 )
        {
          v25 = v41;
          v26 = v41[29];
          if ( v26 != 0 )
          {
            http_opts_fn_3(v26);
            v25 = v41;
            v41[29] = 0;
          }
          v8 = -2143088606;
          v27 = sceNet_lib_D8722983(v38);
          v25[29] = http_opts_fn_2(v27 + 1);
          v28 = v41[29];
          if ( v28 == 0 )
            goto LABEL_15;
          sceNet_lib_80C9F02A(v28, v38);
          v24 = v39;
        }
        v29 = v36;
        if ( v24 != 0 )
        {
          v30 = v41;
          v31 = v41[30];
          if ( v31 != 0 )
          {
            http_opts_fn_3(v31);
            v30 = v41;
          }
          v8 = -2143088606;
          v32 = sceNet_lib_D8722983(v39);
          v30[30] = http_opts_fn_2(v32 + 1);
          v33 = v41[30];
          if ( v33 == 0 )
            goto LABEL_15;
          sceNet_lib_80C9F02A(v33, v39);
          v29 = v36;
        }
        if ( v29 != 0 )
        {
          v34 = http_io_fn_37(v35[1]);
          v8 = http_io_fn_86(&v43, v34, v36, v40);
          if ( v8 < 0 )
            goto LABEL_15;
          http_opts_fn_7(v41[6], "Host", v43, 0);
          http_opts_fn_3(v43);
          v43 = 0;
        }
        http_opts_fn_3(v6);
        v6 = 0;
        if ( v7 != 0 )
          http_opts_fn_16(v7);
      }
      v14 = v41;
    }
  }
  v8 = http_core_fn_5(v14);
  if ( v8 < 0 )
  {
LABEL_15:
    if ( v7 != 0 )
      http_opts_fn_16(v7);
    if ( v6 != 0 )
      http_opts_fn_3(v6);
    v9 = v41;
    goto LABEL_3;
  }
  if ( v41 != NULL )
  {
    v15 = http_io_fn_89();
    v16 = v41[8];
    v41[8] = v16 - 1;
    http_io_fn_90(v15, v16 - 1, v16);
  }
  return 0;
}


int http_core_fn_8(int a1, int a2, int a3, int a4)
{
  char *v7;
  int v8;
  int v9;
  int result;
  int v11;
  u32 v12[3];

  v7 = "WWW-Authenticate";
  v8 = a2;
  v11 = 0;
  v9 = 0;
  if ( a4 == 401 || (v7 = "Proxy-Authenticate", v8 = a2, result = -2143088603, v9 = 1, a4 == 407) )
  {
    if ( sceParseHttpResponseHeader(v8, a3, v7, &v11, v12) < 0 )
      return -2143088603;
    else
      return http_core_fn_9(a1, v11, v12[0], v9);
  }
  return result;
}


int http_core_fn_9(int *a1, u8 *a2, unsigned int a3, int a4)
{
  int v7;
  int v9;
  char *v10;
  unsigned int v11;
  int v12;
  int v13;
  unsigned int v14;
  u8 *v15;
  u32 *v16;
  u32 *v17;
  int v18;
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
  unsigned int v33;
  int v34;
  int v35;
  int v36;
  int v37;
  int v38;
  int v39; // dc
  int v40;
  int v41;
  int v42;
  char *v43;
  char *v44;
  char *v45;
  char *v46;
  char *v47;
  char *v48;
  int v49;

  v7 = 0;
  v42 = 0;
  v41 = 0;
  if ( a1 == NULL )
    return -2143088130;
  sceKernelMemset(a1, 0, 56);
  if ( a2 == NULL || *a2 == 0 )
    v7 = -2143088540;
  v9 = sceNet_lib_D8722983("Basic");
  if ( sceNet_lib_B9085A96(a2, "Basic", v9) != 0 )
  {
    v38 = sceNet_lib_D8722983("Digest");
    v39 = sceNet_lib_B9085A96(a2, "Digest", v38) != 0;
    v40 = 1;
    if ( v39 )
      return -2143088535;
    if ( a4 == 1 )
      v40 = 3;
    v10 = "Digest";
    *a1 = v40;
  }
  else
  {
    if ( a4 == 1 )
      *a1 = 2;
    else
      *a1 = 0;
    v10 = "Basic";
  }
  v11 = sceNet_lib_D8722983(v10);
  if ( v11 < a3 )
  {
    v43 = "stale";
    v44 = "true";
    v45 = "false";
    v46 = "algorithm";
    v47 = "qop";
    v48 = "domain";
    while ( 1 )
    {
      v12 = sceNet_lib_D8722983("realm");
      v13 = sceNet_lib_B9085A96(&a2[v11], "realm", v12);
      v14 = a3 - v11;
      if ( v13 == 0 )
        break;
      v20 = sceNet_lib_D8722983("nonce");
      v21 = sceNet_lib_B9085A96(&a2[v11], "nonce", v20);
      v14 = a3 - v11;
      if ( v21 == 0 )
      {
        v15 = &a2[v11];
        v16 = a1 + 5;
        v17 = a1 + 6;
        goto LABEL_13;
      }
      v22 = sceNet_lib_D8722983("opaque");
      v23 = sceNet_lib_B9085A96(&a2[v11], "opaque", v22);
      v14 = a3 - v11;
      if ( v23 == 0 )
      {
        v15 = &a2[v11];
        v16 = a1 + 7;
        v17 = a1 + 8;
        goto LABEL_13;
      }
      v24 = sceNet_lib_D8722983(v43);
      if ( sceNet_lib_B9085A96(&a2[v11], v43, v24) != 0 )
      {
        v30 = sceNet_lib_D8722983(v46);
        v31 = sceNet_lib_B9085A96(&a2[v11], v46, v30);
        v14 = a3 - v11;
        if ( v31 != 0 )
        {
          v32 = sceNet_lib_D8722983(v47);
          if ( sceNet_lib_B9085A96(&a2[v11], v47, v32) == 0 )
          {
            v11 += http_core_fn_10(&a2[v11], a3 - v11, a1 + 3, a1 + 4);
            if ( a1[3] != 0 )
            {
              v33 = sceNet_lib_D8722983("auth");
              v18 = v11 < a3;
              if ( v33 >= a1[4] )
                goto LABEL_15;
              v49 = a1[3];
              v34 = sceNet_lib_D8722983("auth");
              v35 = sceNet_lib_B9085A96(v49, "auth", v34);
              v18 = v11 < a3;
              if ( v35 != 0 )
                goto LABEL_15;
              a1[4] = sceNet_lib_D8722983("auth");
            }
            goto LABEL_14;
          }
          v36 = sceNet_lib_D8722983(v48);
          v37 = sceNet_lib_B9085A96(&a2[v11], v48, v36);
          v14 = a3 - v11;
          if ( v37 != 0 )
          {
            ++v11;
            goto LABEL_14;
          }
          v15 = &a2[v11];
          v16 = a1 + 11;
          v17 = a1 + 12;
        }
        else
        {
          v15 = &a2[v11];
          v16 = a1 + 9;
          v17 = a1 + 10;
        }
        goto LABEL_13;
      }
      v25 = http_core_fn_10(&a2[v11], a3 - v11, &v41, &v42);
      v26 = v41;
      v11 += v25;
      v27 = sceNet_lib_D8722983(v44);
      if ( sceNet_lib_B9085A96(v26, v44, v27) != 0 )
      {
        v29 = v41;
        v28 = sceNet_lib_D8722983(v45);
        if ( sceNet_lib_B9085A96(v29, v45, v28) != 0 )
        {
          v18 = v11 < a3;
          goto LABEL_15;
        }
        a1[13] = 0;
      }
      else
      {
        a1[13] = 1;
      }
LABEL_14:
      v18 = v11 < a3;
LABEL_15:
      if ( !v18 )
        return v7;
    }
    v15 = &a2[v11];
    v16 = a1 + 1;
    v17 = a1 + 2;
LABEL_13:
    v11 += http_core_fn_10(v15, v14, v16, v17);
    goto LABEL_14;
  }
  return v7;
}


int (*http_core_fn_10(
        u8 *a1,
        unsigned int a2,
        u32 *a3,
        int *a4))(u8 *a1, u8 *a2, int a3)
{
  int (*i)(u8 *, u8 *, int);
  char *v8;
  int v9;
  int k;
  int v11;
  int v13;
  int j;
  int v15;
  int v16;

  *a3 = 0;
  i = NULL;
  *a4 = 0;
  if ( a2 != 0 )
  {
    i = http_core_fn_1;
    if ( *a1 == 61 )
      goto LABEL_7;
    while ( (unsigned int)i < a2 && *((u8 *)i + (u32)a1) != 61 )
      i = (int (*)(u8 *, u8 *, int))((char *)i + 1);
  }
  i = (int (*)(u8 *, u8 *, int))((char *)i + 1);
LABEL_7:
  if ( (unsigned int)i < a2 )
  {
    v8 = (char *)i + (u32)a1;
    if ( *((u8 *)i + (u32)a1) == 34 )
    {
      i = (int (*)(u8 *, u8 *, int))((char *)i + 1);
      if ( a2 >= (unsigned int)i )
      {
        v13 = (unsigned int)i < a2;
        *a3 = (char *)i + (u32)a1;
        if ( (unsigned int)i < a2 )
        {
          if ( *((u8 *)i + (u32)a1) == 34 )
          {
LABEL_24:
            if ( v13 )
            {
              v15 = *((char *)i + (u32)a1);
              if ( v15 != 44 && v15 != 10 )
              {
                for ( i = (int (*)(u8 *, u8 *, int))((char *)i + 1);
                      (unsigned int)i < a2;
                      i = (int (*)(u8 *, u8 *, int))((char *)i + 1) )
                {
                  v16 = *((char *)i + (u32)a1);
                  if ( v16 == 44 || v16 == 10 )
                    break;
                }
              }
            }
          }
          else
          {
            for ( j = *a4; ; j = *a4 )
            {
              i = (int (*)(u8 *, u8 *, int))((char *)i + 1);
              v13 = (unsigned int)i < a2;
              *a4 = j + 1;
              if ( (unsigned int)i >= a2 )
                break;
              if ( *((u8 *)i + (u32)a1) == 34 )
                goto LABEL_24;
            }
          }
        }
      }
    }
    else
    {
      *a3 = v8;
      v9 = *v8;
      if ( v9 != 44 && v9 != 10 )
      {
        for ( k = *a4; ; k = *a4 )
        {
          i = (int (*)(u8 *, u8 *, int))((char *)i + 1);
          *a4 = k + 1;
          if ( (unsigned int)i >= a2 )
            break;
          v11 = *((char *)i + (u32)a1);
          if ( v11 == 44 || v11 == 10 )
            break;
        }
      }
    }
  }
  return i;
}


int http_core_fn_11(u8 *a1, unsigned int *a2, unsigned int a3, int a4, int a5)
{
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  unsigned int v15;
  int v18;
  u8 *v19;
  unsigned int v20;
  int v21;

  v10 = 0;
  v11 = 1;
  if ( a4 == 0 || a5 == 0 )
    return -2143088130;
  if ( a1 != NULL )
    *a1 = 0;
  else
    v11 = 0;
  v12 = sceNet_lib_D8722983(a4);
  v13 = v12 + sceNet_lib_D8722983(":");
  v14 = v13 + sceNet_lib_D8722983(a5) + 1;
  v15 = sceNet_lib_D8722983("Basic ") + 4 * ((v14 + 1) / 3u) + 1;
  if ( v11 == 0 )
  {
    if ( a2 != NULL )
      *a2 = v15;
    return 0;
  }
  if ( a3 < v15 )
    return -2143088606;
  v18 = http_opts_fn_2(v14);
  v19 = (u8 *)v18;
  if ( v18 != 0 )
  {
    sceKernelMemset(v18, 0, v14);
    sceNet_lib_80C9F02A(v19, a4);
    sceNet_lib_5705F6F9(v19, ":");
    sceNet_lib_5705F6F9(v19, a5);
    sceNet_lib_80C9F02A(a1, "Basic ");
    v21 = sceNet_lib_D8722983("Basic ");
    v20 = sceNet_lib_D8722983(v19);
    a1[v21 + http_core_fn_2(&a1[v21], v19, v20)] = 0;
  }
  else
  {
    v10 = -2143088606;
  }
  if ( v19 != NULL )
    http_opts_fn_3(v19);
  return v10;
}


int http_core_fn_12(int a1, u32 *a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, u32 *a10)
{
  int v16;
  int v17;
  char *v19;
  int SystemTimeLow;
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
  u8 v34[16];
  u8 v35[32];
  u8 v36[32];
  u8 v37[48];
  u8 v38[48];
  u32 v39[4];

  v16 = a1 != 0;
  v39[0] = 0;
  if ( a4 == 0 || a5 == 0 || *a10 == 0 || a10[1] == 0 || a10[2] == 0 )
    return -2143088130;
  v17 = -2143088533;
  switch ( a7 )
  {
    case 0:
      v19 = "GET";
      goto LABEL_9;
    case 1:
      v19 = "POST";
      goto LABEL_9;
    case 2:
      v19 = "HEAD";
      goto LABEL_9;
    case 3:
      v19 = "OPTIONS";
      goto LABEL_9;
    case 4:
      v19 = "PUT";
      goto LABEL_9;
    case 5:
      v19 = "DELETE";
      goto LABEL_9;
    case 6:
      v19 = "TRACE";
      goto LABEL_9;
    case 7:
      v19 = "CONNECT";
LABEL_9:
      if ( a8 != 0 )
        http_core_fn_17(a8, a9, v36);
      SystemTimeLow = sceKernelGetSystemTimeLow();
      http_core_fn_16(SystemTimeLow, v38);
      http_core_fn_14(a10[4], a4, *a10, a5, a10[2], v38, v35);
      sceNet_lib_CF705E46(v34, "%08x", a10[6]);
      v21 = a10[2];
      v22 = a10[1];
      v34[8] = 0;
      http_core_fn_15(v35, v21, v34, v38, v22, v19, a6, v36, v37);
      v23 = sceNet_lib_D8722983("Digest ");
      if ( v16 )
        sceNet_lib_B5CE388A(a1, "Digest ", a3);
      v17 = http_cookie_fn_7(a1 + v23, a3 - v23, v39, "username", a4, a1 != 0);
      if ( v17 < 0 )
        return v17;
      v24 = v23 + v39[0];
      v17 = http_cookie_fn_7(a1 + v24, a3 - v24, v39, "realm", *a10, a1 != 0);
      if ( v17 < 0 )
        return v17;
      v25 = v24 + v39[0];
      v17 = http_cookie_fn_7(a1 + v25, a3 - v25, v39, "nonce", a10[2], a1 != 0);
      if ( v17 < 0 )
        return v17;
      v26 = v25 + v39[0];
      v17 = http_cookie_fn_7(a1 + v26, a3 - v26, v39, "uri", a6, a1 != 0);
      if ( v17 < 0 )
        return v17;
      v27 = v26 + v39[0];
      v17 = http_cookie_fn_7(a1 + v27, a3 - v27, v39, "qop", a10[1], a1 != 0);
      if ( v17 < 0 )
        return v17;
      v28 = v27 + v39[0];
      v17 = http_cookie_fn_7(a1 + v28, a3 - v28, v39, "nc", v34, a1 != 0);
      if ( v17 < 0 )
        return v17;
      v29 = v28 + v39[0];
      v17 = http_cookie_fn_7(a1 + v29, a3 - v29, v39, "cnonce", v38, a1 != 0);
      if ( v17 < 0 )
        return v17;
      v30 = v29 + v39[0];
      v17 = http_cookie_fn_7(a1 + v30, a3 - v30, v39, "response", v37, a1 != 0);
      if ( v17 < 0 )
        return v17;
      v31 = a10[3];
      v32 = v30 + v39[0];
      if ( v31 == 0 || sceNet_lib_D8722983(v31) == 0 )
        goto LABEL_22;
      v17 = http_cookie_fn_7(a1 + v32, a3 - v32, v39, "opaque", a10[3], a1 != 0);
      if ( v17 >= 0 )
      {
        v32 += v39[0];
LABEL_22:
        if ( v16 )
          *(u8 *)(a1 + v32) = 0;
        v33 = v32 + 1;
        if ( a2 != NULL )
          *a2 = v33;
      }
      break;
    default:
      return v17;
  }
  return v17;
}


int http_core_fn_13(int *a1, u32 *a2)
{
  unsigned int v4;
  int v5;
  int v6;
  u8 *v7;
  int v8;
  int v9;
  int v10;
  char *v11;
  int v12;
  char *v13;
  int v14;
  int v15;
  int v16;
  char *v17;
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
  int result;
  char *v31;
  int v32;
  u8 *v33;
  int v34;
  unsigned int v35;
  int v36;
  int v37;
  int v38;
  int v39;
  int v40;

  v4 = 0;
  v5 = 0;
  v6 = 0;
  v7 = NULL;
  v8 = 0;
  v9 = 0;
  v40 = 0;
  if ( a2[9] == 0 )
  {
    v5 = 0;
LABEL_3:
    v10 = a2[3];
    goto LABEL_4;
  }
  v37 = http_opts_fn_2(a2[10] + 1);
  v6 = v37;
  if ( v37 == 0 )
    goto LABEL_26;
  sceKernelMemcpy(v37, a2[9], a2[10]);
  *(u8 *)(v6 + a2[10]) = 0;
  v38 = sceNet_lib_D8722983("md5-sess");
  if ( sceNet_lib_B9085A96(v6, "md5-sess", v38) == 0 )
  {
    v5 = 1;
    goto LABEL_3;
  }
  v39 = sceNet_lib_D8722983("md5");
  if ( sceNet_lib_B9085A96(v6, "md5", v39) == 0 )
  {
    v5 = 0;
    goto LABEL_3;
  }
  v10 = a2[3];
LABEL_4:
  if ( v10 != 0 )
  {
    v32 = http_opts_fn_2(a2[4] + 1);
    v7 = (u8 *)v32;
    if ( v32 == 0 )
      goto LABEL_26;
    sceKernelMemcpy(v32, a2[3], a2[4]);
    v7[a2[4]] = 0;
    if ( a2[4] != 0 )
    {
      v33 = v7;
      do
      {
        v34 = sceNet_lib_D8722983("auth-int");
        if ( sceNet_lib_B9085A96(v33, "auth-int", v34) != 0 )
        {
          v36 = sceNet_lib_D8722983("auth");
          if ( sceNet_lib_B9085A96(v33, "auth", v36) == 0 )
            v8 = 1;
        }
        else
        {
          v9 = 1;
        }
        v35 = a2[4];
        if ( v4 < v35 && *v33 != 44 )
        {
          ++v4;
          while ( 1 )
          {
            ++v33;
            if ( v4 >= v35 || *v33 == 44 )
              break;
            ++v4;
          }
        }
      }
      while ( v4 < v35 );
    }
  }
  v11 = "md5-sess";
  if ( v5 != 1 )
    v11 = "md5";
  v12 = sceNet_lib_D8722983(v11) + 1;
  v13 = "auth";
  if ( v9 != 1 )
  {
    if ( v8 != 1 )
      goto LABEL_10;
    v13 = "auth-int";
  }
  v12 += sceNet_lib_D8722983(v13) + 1;
LABEL_10:
  v14 = v12 + a2[10] + a2[12] + a2[6] + a2[8] + a2[4] + a2[2] + 6;
  v15 = http_opts_fn_2(v14);
  v16 = v15;
  if ( v15 != 0 )
  {
    sceKernelMemset(v15, 0, v14);
    if ( v5 == 1 )
    {
      v17 = "md5-sess";
      a1[4] = v16;
    }
    else
    {
      a1[4] = v16;
      v17 = "md5";
    }
    sceNet_lib_80C9F02A(v16, v17);
    v18 = sceNet_lib_D8722983(v17) + 1;
    if ( v9 == 1 )
    {
      v19 = v16 + v18;
      v31 = "auth-int";
      a1[1] = v16 + v18;
    }
    else
    {
      v19 = v16 + v18;
      if ( v8 != 1 )
      {
        a1[1] = 0;
LABEL_16:
        a1[4] = v16 + v18;
        sceKernelMemcpy(v16 + v18, a2[9], a2[10]);
        v20 = a2[12];
        v21 = v18 + a2[10] + 1;
        a1[5] = v16 + v21;
        sceKernelMemcpy(v16 + v21, a2[11], v20);
        v22 = a2[6];
        v23 = v21 + a2[12] + 1;
        a1[2] = v16 + v23;
        sceKernelMemcpy(v16 + v23, a2[5], v22);
        v24 = a2[8];
        v25 = v23 + a2[6] + 1;
        a1[3] = v16 + v25;
        sceKernelMemcpy(v16 + v25, a2[7], v24);
        v26 = a2[4];
        v27 = v25 + a2[8] + 1;
        a1[1] = v16 + v27;
        sceKernelMemcpy(v16 + v27, a2[3], v26);
        v28 = a2[2];
        v29 = v16 + v27 + a2[4] + 1;
        *a1 = v29;
        sceKernelMemcpy(v29, a2[1], v28);
        a1[7] = v16;
        a1[6] = 1;
        goto LABEL_17;
      }
      a1[1] = v19;
      v31 = "auth";
    }
    sceNet_lib_80C9F02A(v19, v31);
    v18 += sceNet_lib_D8722983(v31) + 1;
    goto LABEL_16;
  }
LABEL_26:
  v40 = -2143088606;
LABEL_17:
  if ( v6 != 0 )
    http_opts_fn_3(v6);
  result = v40;
  if ( v7 != NULL )
  {
    http_opts_fn_3(v7);
    return v40;
  }
  return result;
}


int http_core_fn_14(int a1, int a2, int a3, int a4, int a5, int a6, int a7)
{
  int v14;
  int v15;
  int v16;
  int v18;
  int v19;
  u8 v20[96];
  u8 v21[16];

  http_io_fn_92(v20);
  v14 = sceNet_lib_D8722983(a2);
  http_io_fn_93(v20, a2, v14);
  http_io_fn_93(v20, ":", 1);
  v15 = sceNet_lib_D8722983(a3);
  http_io_fn_93(v20, a3, v15);
  http_io_fn_93(v20, ":", 1);
  v16 = sceNet_lib_D8722983(a4);
  http_io_fn_93(v20, a4, v16);
  http_io_fn_94(v20, v21);
  if ( sceNet_lib_9CFBC7E3(a1, "md5-sess") == 0 )
  {
    http_io_fn_92(v20);
    http_io_fn_93(v20, v21, 16);
    http_io_fn_93(v20, ":", 1);
    v18 = sceNet_lib_D8722983(a5);
    http_io_fn_93(v20, a5, v18);
    http_io_fn_93(v20, ":", 1);
    v19 = sceNet_lib_D8722983(a6);
    http_io_fn_93(v20, a6, v19);
    http_io_fn_94(v20, v21);
  }
  return http_cookie_fn_8(v21, a7);
}


int http_core_fn_15(int a1, int a2, int a3, int a4, u8 *a5, int a6, int a7, int a8, int a9)
{
  int v17;
  int v18;
  int v19;
  int v21;
  int v22;
  int v23;
  u8 v24[96];
  u8 v25[16];
  u8 v26[16];
  u8 v27[48];

  http_io_fn_92(v24);
  v17 = sceNet_lib_D8722983(a6);
  http_io_fn_93(v24, a6, v17);
  http_io_fn_93(v24, ":", 1);
  v18 = sceNet_lib_D8722983(a7);
  http_io_fn_93(v24, a7, v18);
  if ( sceNet_lib_9CFBC7E3(a5, "auth-int") == 0 )
  {
    http_io_fn_93(v24, ":", 1);
    http_io_fn_93(v24, a8, 32);
  }
  http_io_fn_94(v24, v25);
  http_cookie_fn_8(v25, v27);
  http_io_fn_92(v24);
  http_io_fn_93(v24, a1, 32);
  http_io_fn_93(v24, ":", 1);
  v19 = sceNet_lib_D8722983(a2);
  http_io_fn_93(v24, a2, v19);
  http_io_fn_93(v24, ":", 1);
  if ( *a5 != 0 )
  {
    v21 = sceNet_lib_D8722983(a3);
    http_io_fn_93(v24, a3, v21);
    http_io_fn_93(v24, ":", 1);
    v22 = sceNet_lib_D8722983(a4);
    http_io_fn_93(v24, a4, v22);
    http_io_fn_93(v24, ":", 1);
    v23 = sceNet_lib_D8722983(a5);
    http_io_fn_93(v24, a5, v23);
    http_io_fn_93(v24, ":", 1);
    http_io_fn_93(v24, v27, 32);
    http_io_fn_94(v24, v26);
  }
  return http_cookie_fn_8(v26, a9);
}


int http_core_fn_16(int a1, int a2)
{
  u8 v4[96];
  u8 v5[16];
  u32 v6[4];

  v6[0] = a1;
  http_io_fn_92(v4);
  http_io_fn_93(v4, v6, 4);
  http_io_fn_94(v4, v5);
  return http_cookie_fn_8(v5, a2);
}


int http_core_fn_17(int a1, int a2, int a3)
{
  int result;
  u8 v7[96];
  u8 v8[16];

  result = -2143088130;
  if ( a1 != 0 && a3 != 0 )
  {
    http_io_fn_92(v7);
    http_io_fn_93(v7, a1, a2);
    http_io_fn_94(v7, v8);
    http_cookie_fn_8(v8, a3);
    return 0;
  }
  return result;
}


int sceHttpFlushAuthList()
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
    if ( HTTP_BSS_U32(0x0) == 1 )
    {
      v2 = http_io_fn_89();
      v5 = HTTP_BSS_U32(0x2648);
      v6 = v2;
      if ( HTTP_BSS_U32(0x2648) != 0 )
      {
        do
        {
          v7 = *(u32 *)(v5 + 604);
          http_core_fn_20();
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


int http_core_fn_18()
{
  int v0;
  unsigned int v1;
  int v2;

  v0 = 0;
  if ( (&HTTP_BSS_U32(0x0))[0] == 1 )
    return -2143088608;
  (&HTTP_BSS_U32(0x0))[0] = 1;
  sceKernelMemset(&HTTP_BSS_U8(0x4), 0, 9792);
  v1 = 0;
  HTTP_BSS_U32(0x2644) = (int)&HTTP_BSS_U8(0x4);
  v2 = 0;
  do
  {
    ++v1;
    (&HTTP_BSS_U32(0x0))[v2 + 153] = (int)&HTTP_BSS_U8(0x268) + v2 * 4;
    v2 += 153;
  }
  while ( v1 < 0xF );
  HTTP_BSS_U32(0x264C) = 0;
  HTTP_BSS_U32(0x2640) = 0;
  HTTP_BSS_U32(0x2648) = 0;
  return v0;
}


int http_core_fn_19()
{
  int v0;
  u32 *i;
  int v3;

  v0 = 0;
  if ( (&HTTP_BSS_U32(0x0))[0] != 1 )
    return -2143088639;
  for ( i = (u32 *)HTTP_BSS_U32(0x2648); i != NULL; i = (u32 *)i[151] )
  {
    http_opts_fn_3(i[6]);
    http_opts_fn_3(i[7]);
    v3 = i[149];
    if ( v3 != 0 )
      http_opts_fn_3(v3);
  }
  (&HTTP_BSS_U32(0x0))[0] = 0;
  sceKernelMemset(&HTTP_BSS_U8(0x4), 0, 9792);
  HTTP_BSS_U32(0x264C) = 0;
  HTTP_BSS_U32(0x2644) = 0;
  HTTP_BSS_U32(0x2648) = 0;
  return v0;
}


void http_core_fn_20(u32 *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;

  v2 = a1[1];
  a1[141] = 1;
  if ( v2 == 0 )
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


void http_core_fn_21(int a1)
{
  int v2;
  int v3;

  if ( *(u32 *)(a1 + 224) != 0 && *(u32 *)(a1 + 944) == 0 )
  {
    v2 = http_core_fn_23();
    *(u32 *)(a1 + 944) = v2;
    if ( v2 != 0 )
    {
      v3 = *(u32 *)(v2 + 12);
      if ( v3 != 0 )
      {
        if ( v3 == 1 )
        {
          ++*(u32 *)(v2 + 592);
          http_io_fn_77(a1, v2 + 48, v2 + 304, 0, 0, v2 + 568, 0);
        }
      }
      else
      {
        http_io_fn_76(a1, v2 + 48, v2 + 304, 0);
      }
    }
  }
}


void http_core_fn_22(int a1, int a2)
{
  int v3;
  int v4;

  if ( *(u32 *)(a2 + 240) != 0 && *(u16 *)(a1 + 310) != 0 && *(u32 *)(a1 + 948) == 0 )
  {
    v3 = http_core_fn_23();
    *(u32 *)(a1 + 948) = v3;
    if ( v3 != 0 )
    {
      v4 = *(u32 *)(v3 + 12);
      if ( v4 == 2 )
      {
        http_io_fn_76(a1, v3 + 48, v3 + 304, 1);
      }
      else if ( v4 == 3 )
      {
        http_io_fn_77(a1, v3 + 48, v3 + 304, 0, 0, v3 + 568, 1);
      }
    }
  }
}


int *http_core_fn_23(u32 *a1, int a2)
{
  int v4;
  int *v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int i;
  int v11;
  int v12;
  int v13;
  int v14;
  int v16;
  int v17;
  unsigned int v18;

  v4 = a1[233];
  v5 = NULL;
  if ( v4 == 0 )
    v4 = a1[232];
  v8 = http_io_fn_89();
  v9 = HTTP_BSS_U32(0x2648);
  if ( HTTP_BSS_U32(0x2648) != 0 )
  {
    for ( i = *(u32 *)(HTTP_BSS_U32(0x2648) + 564); ; i = *(u32 *)(v9 + 564) )
    {
      if ( i == 1 )
      {
        v9 = *(u32 *)(v9 + 604);
        goto LABEL_9;
      }
      if ( *(u32 *)(v9 + 8) != 2 )
        goto LABEL_8;
      if ( a2 == 1 )
      {
        v17 = *(u32 *)(v9 + 12);
        v7 = v17 - 2;
        v6 = (unsigned int)(v17 - 2) < 2;
        if ( (unsigned int)(v17 - 2) < 2 )
        {
          if ( *(u16 *)(v4 + 348) == *(u16 *)(v9 + 20) )
          {
            if ( sceNet_lib_A0F16ABD(*(u32 *)(v4 + 240), *(u32 *)(v9 + 24)) == 0 )
            {
              v5 = (int *)v9;
              goto LABEL_8;
            }
            v9 = *(u32 *)(v9 + 604);
          }
          else
          {
            v9 = *(u32 *)(v9 + 604);
          }
        }
        else
        {
          v9 = *(u32 *)(v9 + 604);
        }
      }
      else if ( *(u32 *)(v9 + 12) < 2u )
      {
        if ( http_cookie_fn_10(*(u32 *)(v9 + 28), a1[28]) == 1 )
        {
          if ( *(u32 *)(v4 + 4) == *(u32 *)(v9 + 16) )
          {
            if ( *(u16 *)(v4 + 400) == *(u16 *)(v9 + 20) )
            {
              if ( sceNet_lib_A0F16ABD(*(u32 *)(v4 + 56), *(u32 *)(v9 + 24)) != 0 )
              {
                v9 = *(u32 *)(v9 + 604);
              }
              else
              {
                if ( v5 == NULL )
                {
                  v5 = (int *)v9;
LABEL_8:
                  v9 = *(u32 *)(v9 + 604);
                  goto LABEL_9;
                }
                v18 = sceNet_lib_D8722983(*(u32 *)(v9 + 28));
                if ( v18 >= sceNet_lib_D8722983(v5[7]) )
                {
                  v5 = (int *)v9;
                  goto LABEL_8;
                }
                v9 = *(u32 *)(v9 + 604);
              }
            }
            else
            {
              v9 = *(u32 *)(v9 + 604);
            }
          }
          else
          {
            v9 = *(u32 *)(v9 + 604);
          }
        }
        else
        {
          v9 = *(u32 *)(v9 + 604);
        }
      }
      else
      {
        v9 = *(u32 *)(v9 + 604);
      }
LABEL_9:
      if ( v9 == 0 )
      {
        if ( v5 != NULL )
        {
          v11 = v5[150];
          v12 = *v5 + 1;
          ++v5[1];
          *v5 = v12;
          if ( v11 != 0 )
          {
            *(u32 *)(v11 + 604) = v5[151];
            v16 = v5[151];
            if ( v16 != 0 )
              *(u32 *)(v16 + 600) = v11;
            else
              HTTP_BSS_U32(0x264C) = v11;
          }
          else
          {
            HTTP_BSS_U32(0x2648) = v5[151];
            v13 = v5[151];
            if ( v13 != 0 )
              *(u32 *)(v13 + 600) = 0;
            else
              HTTP_BSS_U32(0x264C) = 0;
          }
          v5[150] = 0;
          v14 = HTTP_BSS_U32(0x2648);
          v5[151] = HTTP_BSS_U32(0x2648);
          if ( v14 != 0 )
            *(u32 *)(v14 + 600) = v5;
          else
            HTTP_BSS_U32(0x264C) = (int)v5;
          HTTP_BSS_U32(0x2648) = (int)v5;
        }
        break;
      }
    }
  }
  http_io_fn_90(v8, v6, v7);
  return v5;
}


