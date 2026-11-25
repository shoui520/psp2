#include "regcam_internal.h"

int sceNpCommerce2RegCam_D73CC389(
        int a1,
        int (*a2)(),
        int a3,
        int a4,
        int a5,
        unsigned int a6,
        int a7,
        int a8,
        u32 *a9)
{
  int v11;
  s64 v16;
  int v17;
  int v18;
  int result;
  int v20;
  int v21;
  int (*i)();
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  const char *v32;
  int v33;
  int v34;
  int v35;
  int v36;
  int v37;
  unsigned int v38;
  int v39;
  char v40[32];
  u8 v41[44];
  const char *v42;
  int v43;
  u8 *v44;
  int v45;
  const char *v46;
  int v47;
  const char *v48;
  int v49;
  u8 v50[64];
  u8 v51[64];
  u8 v52[32];
  u8 v53[64];
  u8 v54[16];
  u8 v55[112];
  u8 v56[16];
  u8 v57[16];
  u8 v58[48];
  u8 v59[40];
  int v60;
  u8 v61[40];
  int v62;
  u8 v63[2048];
  int v64;
  u8 v65[4];
  int v66;
  int v67;
  int v68;

  v11 = 0;
  v66 = a5;
  g_quick_abort_flag = 0;
  v67 = 0;
  if ( a2 == NULL || a3 == 0 || a4 == 0 || a5 == 0 )
    goto LABEL_8;
  v16 = ((u64)(v16) & 0xFFFFFFFF00000000ULL) | (u32)(a7 == 0);
  v16 = ((u64)(v16) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(a8 == 0) << 32);
  if ( v16 == 0 )
  {
    if ( a9 != NULL )
    {
      v17 = -2141909245;
      if ( a6 < 0x1F )
        goto LABEL_9;
      if ( sceNet_lib_D8722983(a3) == 2 && sceNet_lib_D8722983(a4) == 2 )
      {
        v17 = sceNpCommerce2_460A1D92(v54, 9);
        if ( v17 < 0 )
          goto LABEL_9;
        v17 = -2141909037;
        if ( sceNet_lib_B9085A96((int)"np", v54, 3) == 0 )
          goto LABEL_9;
        v17 = regcam_api_helper_15(a1, 5);
        if ( v17 < 0 )
          goto LABEL_9;
        v17 = sceNpCommerce2RegCam_06B1824F(a1, (int)&g_quick_req_id);
        if ( v17 >= 0 )
        {
          v17 = sceNpCommerce2RegCam_7A315723(g_quick_req_id, a3, a4, 1970, 1, 1);
          if ( v17 >= 0 )
            v17 = sceNpCommerce2RegCam_4B1A9E4E(g_quick_req_id, a7, a8, (int)&v64);
          regcam_api_helper_1(g_quick_req_id);
          g_quick_req_id = 0;
        }
        if ( v17 < 0 )
          goto LABEL_9;
        v20 = sceNpCommerce2RegCam_856C52E9((int)v58, a7, v64);
        v67 = 1;
        if ( v20 >= 0 )
          v20 = sceNpCommerce2RegCam_BD48E118((int)v58, (int)v55);
        v17 = v20;
        if ( v20 < 0 )
          goto LABEL_9;
        v17 = -2141909038;
        if ( g_quick_abort_flag == v67 )
          goto LABEL_9;
        v17 = sceNpCommerce2RegCam_DB478D24(a1, (int)&g_quick_req_id);
        if ( v17 >= 0 )
        {
          v17 = sceNpCommerce2RegCam_5F45DF28(g_quick_req_id);
          if ( v17 >= 0 )
            v17 = sceNpCommerce2RegCam_6EA71511(g_quick_req_id, a7, a8, (int)&v64);
          regcam_api_helper_1(g_quick_req_id);
          g_quick_req_id = 0;
        }
        if ( v17 < 0 )
          goto LABEL_9;
        v21 = sceNpCommerce2RegCam_EC918C7E((int)v59, a7, v64);
        if ( v21 >= 0 )
        {
          v17 = -2141909040;
          if ( v60 == 0 )
            goto LABEL_9;
          v21 = sceNpCommerce2RegCam_FAC30D4E((int)v59, 0, (int)v61);
          if ( v21 >= 0 )
            v11 = v62;
        }
        v17 = v21;
        if ( v21 < 0 )
          goto LABEL_9;
        v17 = -2141909038;
        if ( g_quick_abort_flag == 1 )
          goto LABEL_9;
        while ( 2 )
        {
          for ( i = a2; ; i = a2 )
          {
            v17 = regcam_quick_helper_1(i, a3, a4, v40, 17);
            if ( v17 < 0 )
              goto LABEL_9;
            v17 = -2141909039;
            if ( (unsigned int)(sceNet_lib_D8722983(v40) + 25) > 0x40 )
              goto LABEL_9;
            sceNet_lib_CF705E46(v53, "%s%s", v40, "@d0.mail.playstation.net");
            v17 = regcam_quick_helper_2(a2, v52, 31);
            if ( v17 < 0 )
              goto LABEL_9;
            v68 = sceNet_lib_D8722983(v52);
            v17 = sceNpCommerce2RegCam_B56A1200(a1, (int)&g_quick_req_id);
            if ( v17 >= 0 )
            {
              v17 = sceNpCommerce2RegCam_13E0AD53(g_quick_req_id, (int)v53, (int)v52, v11, (int)v52, v68);
              if ( v17 >= 0 )
                v17 = sceNpCommerce2RegCam_F5E2A7B0(g_quick_req_id, a7, a8, (int)&v64);
              regcam_api_helper_1(g_quick_req_id);
              g_quick_req_id = 0;
            }
            if ( v17 != -2141908924 )
              break;
          }
          if ( v17 < 0 )
            goto LABEL_9;
          v17 = -2141909038;
          if ( g_quick_abort_flag == 1 )
            goto LABEL_9;
          v23 = sceNpCommerce2RegCam_69817297(a1, (int)&g_quick_req_id);
          if ( v23 >= 0 )
          {
            v23 = sceNpCommerce2RegCam_BCFD9CC5(g_quick_req_id);
            if ( v23 >= 0 )
              v23 = sceNpCommerce2RegCam_E256E232(g_quick_req_id, a7, a8, (int)&v64);
          }
          v17 = v23;
          regcam_api_helper_1(g_quick_req_id);
          g_quick_req_id = 0;
          if ( v17 < 0 )
            goto LABEL_9;
          v17 = -2141909038;
          if ( g_quick_abort_flag == 1 )
            goto LABEL_9;
          v24 = sceNpCommerce2RegCam_6F0783DA(a1, (int)&g_quick_req_id);
          if ( v24 >= 0 )
          {
            v24 = sceNpCommerce2RegCam_CD9F9F24(g_quick_req_id, (int)v40);
            if ( v24 >= 0 )
              v24 = sceNpCommerce2RegCam_10E44562(g_quick_req_id, a7, a8, (int)&v64);
          }
          v17 = v24;
          regcam_api_helper_1(g_quick_req_id);
          g_quick_req_id = 0;
          if ( v17 == -2141908920 )
            continue;
          break;
        }
        if ( v17 < 0 )
          goto LABEL_9;
        v17 = -2141909038;
        if ( g_quick_abort_flag == 1 )
          goto LABEL_9;
        v25 = sceNpCommerce2RegCam_B4CC8AE6(a1, (int)&g_quick_req_id);
        if ( v25 >= 0 )
        {
          v27 = sceNet_lib_D8722983("firstName");
          v26 = sceNet_lib_D8722983("lastName");
          v25 = sceNpCommerce2RegCam_EE818631(g_quick_req_id, (int)"firstName", v27, (int)"lastName", v26, 0, 0, 0);
          if ( v25 >= 0 )
            v25 = sceNpCommerce2RegCam_F6415A7A(g_quick_req_id, a7, a8, (int)&v64);
        }
        v17 = v25;
        regcam_api_helper_1(g_quick_req_id);
        g_quick_req_id = 0;
        if ( v17 < 0 )
          goto LABEL_9;
        v17 = -2141909038;
        if ( g_quick_abort_flag == 1 )
          goto LABEL_9;
        v28 = regcam_api_helper_6(a1, (int)&g_quick_req_id);
        if ( v28 >= 0 )
        {
          v28 = regcam_api_helper_7(g_quick_req_id);
          if ( v28 >= 0 )
            v28 = regcam_api_helper_8(g_quick_req_id, a7, a8, (int)&v64);
        }
        v17 = v28;
        regcam_api_helper_1(g_quick_req_id);
        g_quick_req_id = 0;
        if ( (((unsigned int)v17 >> 31) & (v17 != 0x80551004)) != 0 )
          goto LABEL_9;
        v17 = regcam_quick_helper_3(a7, v64, v17 != (int)0x80551004, (int)v50, 64, (int)v51, 64, (int)v56, 16);
        if ( v17 < 0 )
          goto LABEL_9;
        v17 = -2141909038;
        if ( g_quick_abort_flag == 1 )
          goto LABEL_9;
        sceKernelMemset(v41, 0, 92);
        v42 = "StreetAddress1";
        v29 = sceNet_lib_D8722983("StreetAddress1");
        v46 = v50;
        v43 = v29;
        v47 = sceNet_lib_D8722983(v50);
        v44 = v51;
        v30 = sceNet_lib_D8722983(v51);
        v48 = v56;
        v45 = v30;
        v49 = sceNet_lib_D8722983(v56);
        if ( sceNet_lib_B9085A96("US", a3, 3) != 0 )
        {
          if ( sceNet_lib_B9085A96("CA", a3, 3) != 0 )
            goto LABEL_79;
          v46 = "ON";
          v31 = sceNet_lib_D8722983("ON");
          v32 = "M2H 3R6";
        }
        else
        {
          v46 = "CA";
          v31 = sceNet_lib_D8722983("CA");
          v32 = "94404";
        }
        v47 = v31;
        v48 = v32;
        v49 = sceNet_lib_D8722983(v32);
LABEL_79:
        v33 = sceNpCommerce2RegCam_3328C751(a1, (int)&g_quick_req_id);
        if ( v33 >= 0 )
        {
          v33 = sceNpCommerce2RegCam_461DD73D(g_quick_req_id, (int)v41);
          if ( v33 >= 0 )
            v33 = sceNpCommerce2RegCam_28541A7A(g_quick_req_id, a7, a8, (int)&v64);
        }
        v17 = v33;
        regcam_api_helper_1(g_quick_req_id);
        g_quick_req_id = 0;
        if ( v17 >= 0 )
        {
          v17 = -2141909038;
          if ( g_quick_abort_flag != 1 )
          {
            if ( sceNet_lib_B9085A96("KR", a3, 3) == 0 )
            {
              v38 = a2();
              sceNet_lib_CF705E46(v57, "%07u", v38 % 0x989680);
              v39 = sceNpCommerce2RegCam_DF407CA3(a1, (int)&g_quick_req_id);
              if ( v39 >= 0 )
              {
                v39 = sceNpCommerce2RegCam_0EE4DB04(g_quick_req_id, (int)"700101", (int)v57);
                if ( v39 >= 0 )
                  v39 = sceNpCommerce2RegCam_993C822E(g_quick_req_id, a7, a8, (int)&v64);
              }
              v17 = v39;
              regcam_api_helper_1(g_quick_req_id);
              g_quick_req_id = 0;
              if ( v17 < 0 )
              {
                v18 = v67;
                goto LABEL_10;
              }
            }
            v17 = -2141909038;
            if ( g_quick_abort_flag != 1 )
            {
              v34 = sceNpCommerce2RegCam_D99AEC05(a1, (int)&g_quick_req_id);
              if ( v34 >= 0 )
              {
                v34 = sceNpCommerce2RegCam_726FF626(g_quick_req_id, 1, 1);
                if ( v34 >= 0 )
                  v34 = sceNpCommerce2RegCam_FA13A517(g_quick_req_id, a7, a8, (int)&v64);
              }
              v17 = v34;
              regcam_api_helper_1(g_quick_req_id);
              g_quick_req_id = 0;
              if ( v17 >= 0 )
              {
                v17 = -2141909038;
                if ( g_quick_abort_flag != 1 )
                {
                  v35 = sceNpCommerce2RegCam_64D92E9C(a1, (int)&g_quick_req_id);
                  if ( v35 >= 0 )
                  {
                    v35 = sceNpCommerce2RegCam_130756C8(g_quick_req_id);
                    if ( v35 >= 0 )
                      v35 = sceNpCommerce2RegCam_A2A91596(g_quick_req_id, a7, a8, (int)&v64);
                  }
                  v17 = v35;
                  regcam_api_helper_1(g_quick_req_id);
                  g_quick_req_id = 0;
                  if ( v17 >= 0 )
                  {
                    if ( g_quick_abort_flag == 1 )
                    {
                      v17 = -2141909038;
                    }
                    else
                    {
                      v36 = v66;
                      *a9 = v64;
                      sceNet_lib_80C9F02A(v36, v52);
                      if ( (v55[76] & 1) != 0 )
                      {
                        v37 = regcam_quick_helper_4(a1, a3, v53, v52, v41);
                        v17 = v37;
                        if ( v37 < 0 )
                        {
                          v18 = v67;
                          goto LABEL_10;
                        }
                      }
                      if ( g_quick_abort_flag == 1 )
                        v17 = -2141909038;
                    }
                  }
                }
              }
            }
          }
        }
        goto LABEL_9;
      }
    }
LABEL_8:
    v17 = -2141909245;
    goto LABEL_9;
  }
  v17 = -2141909245;
LABEL_9:
  v18 = v67;
LABEL_10:
  if ( v18 != 1 )
    return v17;
  result = v17;
  if ( g_quick_abort_flag == 0 )
  {
    if ( regcam_api_helper_3(a1, (int)&g_quick_req_id) >= 0 && regcam_api_helper_4(g_quick_req_id) >= 0 )
      regcam_api_helper_5(g_quick_req_id, (int)v63, 2048, (int)v65);
    regcam_api_helper_1(g_quick_req_id);
    g_quick_req_id = 0;
    return v17;
  }
  return result;
}


int sceNpCommerce2RegCam_107C4508()
{
  if ( g_quick_req_id != 0 )
    regcam_api_helper_2(g_quick_req_id);
  g_quick_abort_flag = 1;
  return 0;
}


int regcam_quick_helper_1(int (*a1)(), char *a2, char *a3, int a4, unsigned int a5)
{
  int v5;
  int v10;
  u8 *v11;
  unsigned int v12;
  unsigned int v13;
  char v14;
  u8 *v15;
  u32 v17[4];

  v5 = -2141909039;
  if ( a5 >= 0x11 )
  {
LABEL_2:
    v10 = g_quick_abort_flag;
    do
    {
      v5 = -2141909038;
      if ( v10 == 1 )
        break;
      sceNet_lib_80C9F02A(a4, "Q-");
      v11 = (u8 *)(a4 + sceNet_lib_D8722983("Q-"));
      v12 = 0;
      do
      {
        ++v12;
        v13 = a1() % 0x3Eu;
        v14 = v13 + 48;
        if ( v13 >= 0xA )
        {
          v14 = v13 + 55;
          if ( v13 - 10 >= 0x1A )
            v14 = v13 + 61;
        }
        *v11++ = v14;
      }
      while ( v12 < 8 );
      *v11 = 45;
      v15 = v11 + 1;
      *v15 = sceNet_lib_C13C9307(*a2);
      v15[1] = sceNet_lib_C13C9307(a2[1]);
      v15[2] = 45;
      v15[3] = sceNet_lib_C13C9307(*a3);
      v15[4] = sceNet_lib_C13C9307(a3[1]);
      v15[5] = 0;
      v17[0] = 0;
      v5 = sceNpCommerce2RegCam_5F271C1B(16, a4, v17);
      if ( v5 < 0 )
        goto LABEL_2;
      v10 = g_quick_abort_flag;
    }
    while ( v17[0] != 0 );
  }
  return v5;
}


int regcam_quick_helper_2(int (*a1)(), int a2, unsigned int a3)
{
  int v3;
  int v5;
  int v8;
  u8 *v9;
  unsigned int v10;
  unsigned int v11;
  char v12;
  u32 v14[4];

  v3 = a3 < 0x1F;
  v5 = -2141909039;
  if ( !v3 )
  {
LABEL_2:
    v8 = g_quick_abort_flag;
    do
    {
      v9 = (u8 *)a2;
      v5 = -2141909038;
      if ( v8 == 1 )
        break;
      sceKernelMemset(a2, 0, a3);
      v10 = 0;
      do
      {
        ++v10;
        v11 = a1() % 0x3Eu;
        v12 = v11 + 48;
        if ( v11 >= 0xA )
        {
          v12 = v11 + 55;
          if ( v11 - 10 >= 0x1A )
            v12 = v11 + 61;
        }
        *v9++ = v12;
      }
      while ( v10 < 0x1E );
      v14[0] = 0;
      v5 = sceNpCommerce2RegCam_5F271C1B(40, a2, v14);
      if ( v5 < 0 )
        goto LABEL_2;
      v8 = g_quick_abort_flag;
    }
    while ( v14[0] != 0 );
  }
  return v5;
}


int regcam_quick_helper_3(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)
{
  int v17;
  u32 v19[12];
  u8 v20[44];
  int v21;
  int v22;
  u8 v23[44];
  int v24;

  sceKernelMemset(a4, a5, 0);
  sceNet_lib_B5CE388A(a4, "Province", a5);
  sceKernelMemset(a6, a7, 0);
  sceNet_lib_B5CE388A(a6, "City", a7);
  sceKernelMemset(a8, a9, 0);
  sceNet_lib_B5CE388A(a8, "11111", a9);
  v17 = 0;
  if ( a3 != 0 )
  {
    v17 = regcam_api_helper_9((int)v19, a1, a2);
    if ( v17 >= 0 && v19[10] != 0 )
    {
      v17 = regcam_api_helper_10((int)v19, 0, (int)v20);
      if ( v17 >= 0 )
      {
        sceNet_lib_B5CE388A(a4, v21, a5);
        if ( v22 != 0 )
        {
          v17 = regcam_api_helper_11((int)v20, 0, (int)v23);
          if ( v17 >= 0 )
            sceNet_lib_B5CE388A(a6, v24, a7);
        }
      }
    }
  }
  return v17;
}


int regcam_quick_helper_4(int a1, int a2, int a3, int a4, u32 *a5)
{
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  const char *v19;
  int v20;
  u32 v22[24];
  u8 v23[2048];
  u8 v24[2048];
  u8 v25[4];
  u8 v26[12];

  v10 = regcam_api_helper_15(a1, 6);
  if ( v10 >= 0 )
  {
    v10 = -2141909038;
    if ( g_quick_abort_flag != 1 )
    {
      v11 = sceNpCommerce2RegCam_B140C5D7(a1, (int)&g_quick_req_id);
      if ( v11 >= 0 )
      {
        v11 = sceNpCommerce2RegCam_19679B00(g_quick_req_id, 1, a3, a4);
        if ( v11 >= 0 )
          v11 = sceNpCommerce2RegCam_9C9E0B84(g_quick_req_id, (int)v23, 2048, (int)v25);
      }
      regcam_api_helper_1(g_quick_req_id);
      v10 = v11;
      g_quick_req_id = 0;
      if ( v11 >= 0 )
      {
        v10 = -2141909038;
        if ( g_quick_abort_flag != 1 )
        {
          sceKernelMemset(v22, 0, 92);
          v12 = a5[12];
          v13 = a5[19];
          v14 = a5[20];
          v15 = a5[22];
          v16 = a5[17];
          v17 = a5[18];
          v18 = a5[21];
          v22[11] = a5[11];
          v22[12] = v12;
          v22[19] = v13;
          v22[20] = v14;
          v22[0] = "VISA";
          v22[17] = v16;
          v22[18] = v17;
          v22[21] = v18;
          v22[22] = v15;
          v22[1] = sceNet_lib_D8722983("VISA");
          v22[2] = "firstName lastName";
          v22[3] = sceNet_lib_D8722983("firstName lastName");
          v22[5] = 2008;
          v22[7] = 2022;
          v22[8] = 12;
          v22[9] = "150";
          v22[6] = 12;
          if ( sceNet_lib_B9085A96("US", a2, 3) != 0 && sceNet_lib_B9085A96("CA", a2, 3) != 0 )
            v19 = "4111111111111111";
          else
            v19 = "4444444444444448";
          v22[4] = v19;
          v20 = regcam_api_helper_12(a1, (int)&g_quick_req_id);
          if ( v20 >= 0 )
          {
            v20 = regcam_api_helper_13(g_quick_req_id, (int)v22);
            if ( v20 >= 0 )
              v20 = regcam_api_helper_14(g_quick_req_id, (int)v24, 2048, (int)v26);
          }
          regcam_api_helper_1(g_quick_req_id);
          g_quick_req_id = 0;
          return v20;
        }
      }
    }
  }
  return v10;
}


