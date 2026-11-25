#include "store_internal.h"

u32 g_store_init = 0;
u32 g_store_mgr = 0;

int sceNpCommerce2Store_8E76A121(int a1, int a2, int a3, int a4, unsigned int *a5, unsigned int a6, unsigned int a7)
{
  int v7;
  s64 v8;
  unsigned int v9;
  unsigned int *v10;
  unsigned int i;
  unsigned int v12;

  v7 = -2141909247;
  if ( g_store_init != 0 )
  {
    v7 = -2141909245;
    if ( (unsigned int)(a2 - 1) < 8 )
    {
      v8 = ((u64)(v8) & 0xFFFFFFFF00000000ULL) | (u32)(a6 != 1);
      v8 = ((u64)(v8) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(a7 >= 8) << 32);
      v7 = -2141909245;
      if ( v8 == 0 )
      {
        v9 = 0;
        if ( a6 != 0 )
        {
          v10 = a5;
          for ( i = *a5; ; i = *v10 )
          {
            ++v9;
            if ( i >= 7 )
              break;
            if ( v10[1] == 0 )
              break;
            v12 = v10[2];
            v10 += 3;
            if ( v12 == 0 )
              break;
            if ( v9 >= a6 )
              return store_util_fn_12();
          }
          return -2141909245;
        }
        else
        {
          return store_util_fn_12();
        }
      }
    }
  }
  return v7;
}


int sceNpCommerce2Store_81044B28(int a1, int a2, int a3, int a4, unsigned int a5, int a6)
{
  int v6;

  v6 = -2141909247;
  if ( g_store_init != 0 )
  {
    if ( a6 == 1 )
    {
      if ( a2 == 0 || a3 == 0 || a5 >= 0x3D )
        return -2141909245;
    }
    else
    {
      if ( a6 != 2 )
        return -2141909245;
      v6 = -2141909245;
      if ( a5 >= 0x3D )
        return v6;
    }
    return store_util_fn_21();
  }
  return v6;
}


int sceNpCommerce2Store_E3D1EA90(int a1)
{
  int v1;
  int v2;
  unsigned int v3;
  int v4;
  u8 *v5;

  v1 = a1;
  v2 = 0;
  v3 = 0;
  if ( a1 != 0 )
  {
    while ( 1 )
    {
      ++v3;
      v4 = sceNet_lib_BCBE14CF(v1, 45);
      if ( v4 == 0 )
        break;
      v1 = v4 + 1;
      if ( v3 >= 2 )
      {
        v5 = (u8 *)sceNet_lib_BCBE14CF(v1, 45);
        if ( v5 != NULL )
          *v5 = 0;
        return v2;
      }
    }
  }
  return -2141909241;
}


int sceNpCommerce2Store_B20C910B(int a1, int a2, unsigned int a3)
{
  int v3;
  int v6;
  unsigned int v8;
  int v9;
  int v10;
  int v11;

  v3 = a1;
  v6 = 0;
  v8 = 0;
  if ( a1 != 0 )
  {
    while ( 1 )
    {
      ++v8;
      v9 = sceNet_lib_BCBE14CF(v3, 45);
      if ( v9 == 0 )
        break;
      v3 = v9 + 1;
      if ( v8 >= 2 )
      {
        v10 = sceNet_lib_BCBE14CF(v3, 45);
        if ( v10 == 0 )
          v10 = a1 + sceNet_lib_D8722983(a1);
        v11 = v10 - a1;
        if ( v10 - a1 >= a3 )
          return -2141909116;
        sceKernelMemcpy(a2, a1, v10 - a1);
        *(u8 *)(a2 + v11) = 0;
        return v6;
      }
    }
  }
  return -2141909241;
}


int sceNpCommerce2Store_DA7B5568(int a1, u32 *a2)
{
  int v3;
  unsigned int v6;
  int v7;
  int v8;
  u8 *v9;
  int v10;
  u8 *v11;
  u8 *v12;

  v3 = 0;
  if ( a1 == 0 || a2 == NULL )
    return -2141909245;
  v6 = sceNet_lib_D8722983(a1);
  v7 = sceNet_lib_D8722983("IP910");
  if ( v6 >= v7 + sceNet_lib_D8722983("-NPIA9000") + 4 )
  {
    v8 = sceNet_lib_D8722983("IP910");
    if ( sceNet_lib_94DCA9F0("IP910", a1, v8) != 0 )
    {
      *a2 = 0;
    }
    else
    {
      v9 = (u8 *)(a1 + sceNet_lib_D8722983("IP910"));
      if ( (u8)(*v9 - 49) < 5u )
      {
        v10 = sceNet_lib_D8722983("-NPIA9000");
        v11 = v9 + 1;
        if ( sceNet_lib_94DCA9F0("-NPIA9000", v11, v10) != 0 )
        {
          *a2 = 0;
        }
        else
        {
          v12 = &v11[sceNet_lib_D8722983("-NPIA9000")];
          *a2 = (u8)(*v12 - 53) < 5u && v12[1] == 95;
        }
      }
      else
      {
        *a2 = 0;
      }
    }
  }
  else
  {
    *a2 = 0;
  }
  return v3;
}


int sceNpCommerce2Store_7B6C5AEB(int a1, u32 *a2)
{
  int v3;
  unsigned int v6;
  int v7;
  int v8;
  u8 *v9;
  int v10;

  v3 = 0;
  if ( a1 == 0 || a2 == NULL )
    return -2141909245;
  v6 = sceNet_lib_D8722983(a1);
  v7 = sceNet_lib_D8722983("IP9100-NPIA0900");
  if ( v6 >= v7 + sceNet_lib_D8722983("_00-CC") + 1 )
  {
    v8 = sceNet_lib_D8722983("IP9100-NPIA0900");
    if ( sceNet_lib_94DCA9F0("IP9100-NPIA0900", a1, v8) != 0 )
    {
      *a2 = 0;
    }
    else
    {
      v9 = (u8 *)(a1 + sceNet_lib_D8722983("IP9100-NPIA0900"));
      if ( (u8)(*v9 - 50) < 5u )
      {
        v10 = sceNet_lib_D8722983("_00-CC");
        if ( sceNet_lib_94DCA9F0("_00-CC", v9 + 1, v10) != 0 )
        {
          *a2 = 0;
        }
        else
        {
          sceNet_lib_D8722983("_00-CC");
          *a2 = 1;
        }
      }
      else
      {
        *a2 = 0;
      }
    }
  }
  else
  {
    *a2 = 0;
  }
  return v3;
}


int sceNpCommerce2Store_59D8A74D()
{
  int v0;
  int v1;

  v0 = -2141909246;
  if ( g_store_init != 0 )
    return v0;
  v0 = store_disp_fn_78(0, 0, 0);
  v1 = 0;
  if ( v0 < 0 )
    return v0;
  g_store_init = 1;
  return v1;
}


int sceNpCommerce2Store_8455D3ED()
{
  if ( g_store_init != 0 )
  {
    store_term_noop();
    g_store_init = 0;
  }
  return 0;
}


int sceNpCommerce2Store_48FE9646(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_tlv_fn_39();
  }
  return v3;
}


int sceNpCommerce2Store_CD27E505(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_40();
  }
  return v3;
}


int sceNpCommerce2Store_873887CF(int a1, int a2, int a3, int a4)
{
  return store_req_fn_1(a1, a2, a3, a4, 0);
}


int sceNpCommerce2Store_2A06624D(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_tlv_fn_41();
  }
  return v3;
}


int sceNpCommerce2Store_45E24DCA(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_42();
  }
  return v3;
}


int sceNpCommerce2Store_D601DE6A(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_43();
  }
  return v2;
}


int sceNpCommerce2Store_7D7FB404(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_33();
  }
  return v2;
}


int sceNpCommerce2Store_ABF975F8(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_34();
  }
  return v2;
}


int sceNpCommerce2Store_28EF259E(int a1, int a2, int a3, int a4, int a5, int a6)
{
  return store_req_fn_2(a1, a2, a3, a4, a5, a6, 0);
}


int sceNpCommerce2Store_98F66F2C(int a1, int a2, int a3, int a4)
{
  return store_req_fn_3(a1, a2, a3, a4, 0);
}


int sceNpCommerce2Store_607146F0(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_10();
  }
  return v2;
}


int sceNpCommerce2Store_B2DDD9D5(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_47();
  }
  return v3;
}


int sceNpCommerce2Store_C228C7BF(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_11();
  }
  return v2;
}


int sceNpCommerce2Store_B93B30CD(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_53();
  }
  return v3;
}


int sceNpCommerce2Store_869624DB(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_16();
  }
  return v3;
}


int sceNpCommerce2Store_4E84A105(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_17();
  }
  return v3;
}


int sceNpCommerce2Store_E7BBC497(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_16();
  }
  return v3;
}


int sceNpCommerce2Store_B477732B(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_16();
  }
  return v3;
}


int sceNpCommerce2Store_F7D8224B(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_16();
  }
  return v3;
}


int sceNpCommerce2Store_219B9CD4(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_16();
  }
  return v3;
}


int sceNpCommerce2Store_A7C4A4E0(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_15();
  }
  return v3;
}


int sceNpCommerce2Store_F5E850B6(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_48();
  }
  return v2;
}


int sceNpCommerce2Store_EEAC383D(int a1, int a2, int a3, int a4, int a5)
{
  return store_req_fn_4(a1, a2, a3, a4, a5, 0);
}


int sceNpCommerce2Store_619AFBAE(int a1, int a2, int a3, int a4)
{
  return store_req_fn_5(a1, a2, a3, a4, 0);
}


int sceNpCommerce2Store_9B001542(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_12();
  }
  return v2;
}


int sceNpCommerce2Store_5CAF925C(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_50();
  }
  return v3;
}


int sceNpCommerce2Store_99A17682(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_13();
  }
  return v3;
}


int sceNpCommerce2Store_4B3A29D4(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_22();
  }
  return v3;
}


int sceNpCommerce2Store_39D8AB1E(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_4();
  }
  return v3;
}


int sceNpCommerce2Store_92393647(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_15();
  }
  return v3;
}


int sceNpCommerce2Store_9DBCFF3D(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_15();
  }
  return v3;
}


int sceNpCommerce2Store_E5ECA4DF(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_51();
  }
  return v3;
}


int sceNpCommerce2Store_783CC832(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_52();
  }
  return v3;
}


int sceNpCommerce2Store_236FC217(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_14();
  }
  return v3;
}


int sceNpCommerce2Store_7A08C301(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_48();
  }
  return v2;
}


int sceNpCommerce2Store_ECF70261(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_80(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_EF8F317D(int a1, int a2, int a3, int a4, int a5)
{
  int v5;

  v5 = -2141909247;
  if ( g_store_init != 0 )
  {
    v5 = -2141909245;
    if ( a2 != 0 && a3 != 0 && (a5 == 0 || a5 == a3) )
      return store_disp_fn_81();
  }
  return v5;
}


int sceNpCommerce2Store_FB5F3C12(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_CCA8601C(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_tlv_fn_54();
  }
  return v3;
}


int sceNpCommerce2Store_5115E0CB(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_1();
  }
  return v2;
}


int sceNpCommerce2Store_5CE497F6(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_2();
  }
  return v2;
}


int sceNpCommerce2Store_61FD9968(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_19();
  }
  return v3;
}


int sceNpCommerce2Store_EFFCA1ED(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_3();
  }
  return v2;
}


int sceNpCommerce2Store_B8E4EED6(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_46();
  }
  return v2;
}


int sceNpCommerce2Store_74B79FF2(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_20();
  }
  return v2;
}


int sceNpCommerce2Store_782ED6EB(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_15();
  }
  return v3;
}


int sceNpCommerce2Store_D2E48887(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_17();
  }
  return v3;
}


int sceNpCommerce2Store_CADDC96B(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_21();
  }
  return v2;
}


int sceNpCommerce2Store_E1776B29(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_22();
  }
  return v3;
}


int sceNpCommerce2Store_264E9CAA(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_4();
  }
  return v3;
}


int sceNpCommerce2Store_F4261841(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_48();
  }
  return v2;
}


int sceNpCommerce2Store_DD8F307B(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_15();
  }
  return v3;
}


int sceNpCommerce2Store_B9133D27(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_82(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_CA9FB11B(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_83(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_4534FD80(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_21134841(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_5();
  }
  return v3;
}


int sceNpCommerce2Store_BE626428(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_6();
  }
  return v2;
}


int sceNpCommerce2Store_C8133F70(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_7();
  }
  return v2;
}


int sceNpCommerce2Store_AE6DAF88(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_84(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_AD64726E(int a1)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_85(a1, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_59E39F46(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_6301672F(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_86(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_701385D5(int a1)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_87(a1, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_850F5E58(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_99300AC2(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_8();
  }
  return v3;
}


int sceNpCommerce2Store_211DFA3C(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_9();
  }
  return v2;
}


int sceNpCommerce2Store_48F4C175(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_10();
  }
  return v2;
}


int sceNpCommerce2Store_41C60662(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_88(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_ECE95BB2(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_90(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_1F4FFF9A(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_4E280B0A(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_11();
  }
  return v3;
}


int sceNpCommerce2Store_E1DC1A08(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_12();
  }
  return v2;
}


int sceNpCommerce2Store_A8B9FFDB(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_14();
  }
  return v2;
}


int sceNpCommerce2Store_694667CF(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_13();
  }
  return v2;
}


int sceNpCommerce2Store_CAB4A8F6(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_89(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_F5381482(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_90(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_B1957179(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_0A7B6B92(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_15();
  }
  return v3;
}


int sceNpCommerce2Store_849E6B93(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_16();
  }
  return v2;
}


int sceNpCommerce2Store_FB5D86D3(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_17();
  }
  return v2;
}


int sceNpCommerce2Store_19FAC163(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_18();
  }
  return v2;
}


int sceNpCommerce2Store_A89F0478(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_91(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_8C0C2F7C(int a1)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_92(a1, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_EBBA8ECE(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_7136C275(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_19();
  }
  return v3;
}


int sceNpCommerce2Store_259AE74B(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_20();
  }
  return v2;
}


int sceNpCommerce2Store_320C18DB(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_93(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_39757926(int a1)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_94(a1, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_6ACDFAC8(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_FFEBE5FF(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_21();
  }
  return v3;
}


int sceNpCommerce2Store_998EB0CC(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_22();
  }
  return v3;
}


int sceNpCommerce2Store_6C5AE9E5(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_23();
  }
  return v3;
}


int sceNpCommerce2Store_73AE0FA0(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_95(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_995ECF93(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_97(a1, a2, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_CBBFF272(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_4D5DA905(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_24();
  }
  return v3;
}


int sceNpCommerce2Store_91749B96(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_25();
  }
  return v3;
}


int sceNpCommerce2Store_B961BF22(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_98(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_1A7F75D7()
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_99();
  return result;
}


int sceNpCommerce2Store_A52FFFF8(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_18457EB9(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_26();
  }
  return v3;
}


int sceNpCommerce2Store_57DD55E5(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_24();
  }
  return v3;
}


int sceNpCommerce2Store_063A16A4(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_25();
  }
  return v2;
}


int sceNpCommerce2Store_CE84A5DC(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_15();
  }
  return v3;
}


int sceNpCommerce2Store_6961822C(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_26();
  }
  return v3;
}


int sceNpCommerce2Store_B1BEAFED(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_100(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_7A1291A6(int a1)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_102(a1, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_E55BC0DF(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_EAF03127(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_tlv_fn_27();
  }
  return v3;
}


int sceNpCommerce2Store_D58BC86C(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_103(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_12378B6A(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a2 != 0 )
    {
      v2 = -2141908950;
      if ( *(u32 *)(a2 + 48) != 0 )
        return store_disp_fn_104();
    }
  }
  return v2;
}


int sceNpCommerce2Store_53716B04(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_8EABFF3E(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_105(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_472E4D84(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_106(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_B0E5DFD8(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_6EB54FA3(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_27();
  }
  return v3;
}


int sceNpCommerce2Store_DA757050(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_107(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_B8889A67(int a1)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_108(a1, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_0E2B656E(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_C7B9E02B(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_28();
  }
  return v3;
}


int sceNpCommerce2Store_8B81197E(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_29();
  }
  return v2;
}


int sceNpCommerce2Store_7B3A8218(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_1(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_D2517B02(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_util_fn_3(a1, a2, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_ECE10E5B(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_49E768E9(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_30();
  }
  return v3;
}


int sceNpCommerce2Store_9AA823E4(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_31();
  }
  return v2;
}


int sceNpCommerce2Store_0A5949EF(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_2(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_C57CF651(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_util_fn_3(a1, a2, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_6FE1F29E(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_346E1F56(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_32();
  }
  return v3;
}


int sceNpCommerce2Store_3572E4D7(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_28();
  }
  return v2;
}


int sceNpCommerce2Store_75C205F7(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_4(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_68B07CDC(int a1, int a2, int a3)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_5(a1, a2, a3, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_334F89A3(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_2E290D53(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_6(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_47A8A5B4(int a1, int a2, int a3, int a4)
{
  int v4;

  v4 = -2141909247;
  if ( g_store_init != 0 )
  {
    v4 = -2141909245;
    if ( a2 != 0 && a3 != 0 && a4 != 0 )
      return store_util_fn_8(a1);
  }
  return v4;
}


int sceNpCommerce2Store_FEDE1E87(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_2180FE6C(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_33();
  }
  return v3;
}


int sceNpCommerce2Store_5EC8E7B8(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_34();
  }
  return v2;
}


int sceNpCommerce2Store_00292401(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_35();
  }
  return v2;
}


int sceNpCommerce2Store_FC6408EC(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_29();
  }
  return v3;
}


int sceNpCommerce2Store_25579A90(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_46();
  }
  return v2;
}


int sceNpCommerce2Store_66BC1072(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_30();
  }
  return v2;
}


int sceNpCommerce2Store_D22ADDA7(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_17();
  }
  return v3;
}


int sceNpCommerce2Store_65D72F22(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_31();
  }
  return v2;
}


int sceNpCommerce2Store_2581BC37(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_15();
  }
  return v3;
}


int sceNpCommerce2Store_59443417(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_51();
  }
  return v3;
}


int sceNpCommerce2Store_1C4CE9CB(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_52();
  }
  return v3;
}


int sceNpCommerce2Store_7063CC9D(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_14();
  }
  return v3;
}


int sceNpCommerce2Store_548C6487(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_tlv_fn_48();
  }
  return v2;
}


int sceNpCommerce2Store_F036856B(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_22();
  }
  return v3;
}


int sceNpCommerce2Store_7F642484(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_4();
  }
  return v3;
}


int sceNpCommerce2Store_9D3888D8(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_18();
  }
  return v3;
}


int sceNpCommerce2Store_92E53CD4(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_7(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_D232FB91(int a1, int a2, int a3, int a4)
{
  int v4;

  v4 = -2141909247;
  if ( g_store_init != 0 )
  {
    v4 = -2141909245;
    if ( a2 != 0 && a3 != 0 && a4 != 0 )
      return store_util_fn_8(a1);
  }
  return v4;
}


int sceNpCommerce2Store_6AECBA79(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_D63D12BF(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_36();
  }
  return v3;
}


int sceNpCommerce2Store_ED2C778E(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_37();
  }
  return v2;
}


int sceNpCommerce2Store_CBC27838(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_38();
  }
  return v2;
}


int sceNpCommerce2Store_7C45D60E(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_101(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_09B3E27D(int a1)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_102(a1, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_8644CCFB(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_770C5947(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_tlv_fn_32();
  }
  return v3;
}


int sceNpCommerce2Store_78BA836A(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_9(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_7D654C92(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_util_fn_10(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_C0BB62AD(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_DDD2C398(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_11(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_D70F1117(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_54D94925(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_39();
  }
  return v3;
}


int sceNpCommerce2Store_94E8DAB3(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_40();
  }
  return v3;
}


int sceNpCommerce2Store_465A6BBA(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_41();
  }
  return v3;
}


int sceNpCommerce2Store_E6009051(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_42();
  }
  return v3;
}


int sceNpCommerce2Store_B1F9111F(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_43();
  }
  return v2;
}


int sceNpCommerce2Store_6DDED3F7(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_disp_fn_96(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_E1A8D7FF(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_97(a1, 0, g_store_init, a4);
  return result;
}


int sceNpCommerce2Store_5857AF83(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_4E3E0594(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_tlv_fn_35();
  }
  return v3;
}


int sceNpCommerce2Store_FBF61E40(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_44();
  }
  return v3;
}


int sceNpCommerce2Store_BC7F8DA6(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_disp_fn_45();
  }
  return v3;
}


int sceNpCommerce2Store_A93E02A6(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_store_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return store_disp_fn_46();
  }
  return v2;
}


int sceNpCommerce2Store_166EA2D5(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_13(a1, a2, 0x20000u);
  }
  return result;
}


int sceNpCommerce2Store_F22C4985(int a1)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_util_fn_14(a1, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_47540138(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
    return store_disp_fn_74(a1, a2, a3, a4, g_store_init, 0x20000u);
  return result;
}


int sceNpCommerce2Store_0E083AEA(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return store_disp_fn_47();
  }
  return v3;
}


int sceNpCommerce2Store_84C4226F(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_store_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return store_tlv_fn_37();
  }
  return v3;
}


int sceNpCommerce2Store_A165F819(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_store_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return store_util_fn_15(a1, a2, 0x20000u);
  }
  return result;
}


