#include "regcam_internal.h"

u32 g_regcam_init = 0;
u32 g_regcam_mgr = 0;
u32 g_quick_req_id = 0;
u32 g_quick_abort_flag = 0;

int sceNpCommerce2RegCam_1EA0D72D(int a1, u16 *a2, u32 *a3)
{
  int v3;
  int v4;
  unsigned int v5;
  int v6;
  u16 *v7;
  unsigned int v8;
  u8 *v9;
  int v10;
  u8 v12[256];

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != NULL && a3 != NULL )
    {
      v4 = *a2;
      *a3 = 0;
      v5 = 0;
      if ( v4 != 0 )
      {
        v6 = 0;
        do
        {
          v7 = &a2[v6];
          v8 = a2[v6];
          v9 = &v12[v5++];
          v6 = v5;
          if ( v8 >= 0x80 )
            *a3 |= 0x80u;
          v10 = a2[v5] != 0 && v5 < 0x100;
          *v9 = *(u8 *)v7;
        }
        while ( v10 );
      }
      v3 = -2141909245;
      if ( v5 != 256 )
      {
        v12[v5] = 0;
        return regcam_validate_field(a1, v12, (int *)a3);
      }
    }
  }
  return v3;
}


int sceNpCommerce2RegCam_78E3D827()
{
  int v0;
  int v1;

  v0 = -2141909246;
  if ( g_regcam_init != 0 )
    return v0;
  v0 = regcam_init_internal();
  v1 = 0;
  if ( v0 < 0 )
    return v0;
  g_regcam_init = 1;
  return v1;
}


int sceNpCommerce2RegCam_0400E86A()
{
  if ( g_regcam_init != 0 )
  {
    regcam_term_internal();
    g_regcam_init = 0;
  }
  return 0;
}


int regcam_api_helper_1(int a1)
{
  int v2;
  int v3;
  int v5;

  if ( sceKernelGetCompiledSdkVersion() <= 84213775 || (v5 = _sceKernelCheckThreadStack(), v3 = -2141913084, v5 >= 4080) )
  {
    v2 = -2141909247;
    if ( g_regcam_init != 0 )
      return regcam_destroy_req_internal(a1);
    return v2;
  }
  return v3;
}


int regcam_api_helper_2(int a1)
{
  int v2;
  int v3;
  int v5;

  if ( sceKernelGetCompiledSdkVersion() <= 84213775 || (v5 = _sceKernelCheckThreadStack(), v3 = -2141913084, v5 >= 4080) )
  {
    v2 = -2141909247;
    if ( g_regcam_init != 0 )
      return regcam_abort_req_internal(a1);
    return v2;
  }
  return v3;
}


int regcam_api_helper_3(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_7(a1, a2);
  }
  return result;
}


int regcam_api_helper_4(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_8(a1);
  return result;
}


int regcam_api_helper_5(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int regcam_api_helper_6(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_10(a1, a2);
  }
  return result;
}


int regcam_api_helper_7(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_11(a1);
  return result;
}


int regcam_api_helper_8(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int regcam_api_helper_9(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_25(a1, a2, a3);
  }
  return v3;
}


int regcam_api_helper_10(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_26(a1, a2, a3);
  }
  return v3;
}


int regcam_api_helper_11(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_27(a1, a2, a3);
  }
  return v3;
}


int regcam_api_helper_12(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_16(a1, a2);
  }
  return result;
}


int regcam_api_helper_13(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a2 != 0 )
    {
      v2 = -2141908950;
      if ( *(u32 *)(a2 + 48) != 0 )
        return regcam_dispatch_17(a1, a2);
    }
  }
  return v2;
}


int regcam_api_helper_14(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_1E1C2D9D(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_18(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_CA669133(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_1(a1);
  return result;
}


int sceNpCommerce2RegCam_D1C2D16F(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_49967C41(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_28(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_38BE19B7(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_4(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_ECB37716(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_19(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_F692E73E(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_dispatch_2(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_CB9EC34C(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_DEA9E110(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_29(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_3E015183(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_5(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_43A10942(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_30(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_06B1824F(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_20(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_7A315723(int a1, int a2, int a3, int a4, int a5, int a6)
{
  int v6;

  v6 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v6 = -2141909245;
    if ( a2 != 0 && a3 != 0 && a4 != 0 && a5 != 0 && a6 != 0 )
      return regcam_dispatch_3(a1, a2, a3, a4, a5, a6);
  }
  return v6;
}


int sceNpCommerce2RegCam_4B1A9E4E(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_856C52E9(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_31(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_BD48E118(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_6(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_DB478D24(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_21(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_5F45DF28(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_22(a1);
  return result;
}


int sceNpCommerce2RegCam_6EA71511(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_EC918C7E(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_32(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_FAC30D4E(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_33(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_B56A1200(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_23(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_13E0AD53(int a1, int a2, int a3, int a4, int a5, int a6)
{
  int v6;

  v6 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v6 = -2141909245;
    if ( a5 != 0 && a6 != 0 )
      return regcam_dispatch_24(a1, a2, a3, a4, a5, a6);
  }
  return v6;
}


int sceNpCommerce2RegCam_F5E2A7B0(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_2054F881(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_25(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_9018A33F(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_26(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_F3D3D32F(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_35685D12(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_27(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_90E218A2(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_28(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_FA305116(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_5EE50062(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_29(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_1A0507A7(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_31(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_D059973F(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_6F0783DA(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_30(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_CD9F9F24(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_31(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_10E44562(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_E68AD08A(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_32(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_DC403471(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_33(a1);
  return result;
}


int sceNpCommerce2RegCam_54E7925E(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_9AFBDA97(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_34(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_29CB32DA(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_7(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_B4CC8AE6(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_34(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_EE818631(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
  int v5;

  v5 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v5 = -2141909245;
    if ( a2 != 0 && a3 != 0 && a4 != 0 && a5 != 0 )
      return regcam_dispatch_35(a1, a2, a3, a4, a5, a6, a7, a8);
  }
  return v5;
}


int sceNpCommerce2RegCam_F6415A7A(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_3328C751(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_36(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_461DD73D(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a2 != 0 )
    {
      v2 = -2141908950;
      if ( *(u32 *)(a2 + 48) != 0 )
        return regcam_dispatch_37(a1, a2);
    }
  }
  return v2;
}


int sceNpCommerce2RegCam_28541A7A(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_DF407CA3(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_38(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_0EE4DB04(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_dispatch_39(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_993C822E(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_7D3AD059(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_40(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_505C1C2B(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_41(a1);
  return result;
}


int sceNpCommerce2RegCam_544545B6(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_F8BA328E(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_35(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_6BFAB2ED(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_36(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_D99AEC05(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_42(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_726FF626(int a1, int a2, int a3)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_43(a1, a2, a3);
  return result;
}


int sceNpCommerce2RegCam_FA13A517(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_289E304C(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_44(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_BD9DD225(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_dispatch_45(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_B816C674(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_AA077B50(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_46(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_C1D499F5(int a1, int a2, int a3, int a4, int a5, int a6)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_47(a1, a2, a3, a4, a5, a6);
  }
  return result;
}


int sceNpCommerce2RegCam_CE4F0A96(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_32016B1A(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_48(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_68315782(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_dispatch_49(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_E8B01984(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_1C3B41DA(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_50(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_4D7EEABD(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_51(a1);
  return result;
}


int sceNpCommerce2RegCam_3EFA6523(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_14626EC6(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_37(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_37F3D00E(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_8(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_3B9334BA(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_52(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_EF397E50(int a1, int a2, int a3)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_53(a1, a2, a3);
  return result;
}


int sceNpCommerce2RegCam_7AB45618(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_64D92E9C(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_54(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_130756C8(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_55(a1);
  return result;
}


int sceNpCommerce2RegCam_A2A91596(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_C0CAEE74(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_38(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_F5BCBE33(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_5(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_B140C5D7(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_56(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_19679B00(int a1, int a2, int a3, int a4)
{
  int v4;

  v4 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    if ( a2 != 1 )
      return regcam_dispatch_4(a1, a2, a3, a4);
    v4 = -2141909245;
    if ( a3 != 0 && a4 != 0 )
      return regcam_dispatch_4(a1, a2, a3, a4);
  }
  return v4;
}


int sceNpCommerce2RegCam_9C9E0B84(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_BBAE6864(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_9(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_43ABA1B2(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_5(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_6CDE3C3A(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_6(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_DA4D6AC6(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_39(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_C8124194(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_10(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_17843035(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_40(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_B0EEB010(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_57(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_4328E348(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_9(a1);
  return result;
}


int sceNpCommerce2RegCam_8061F181(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_B4EE6FD8(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_58(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_53BB97C3(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
{
  int v9;

  v9 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v9 = -2141909245;
    if ( (a2 != 1 || a3 != 0) && a4 != 0 && a5 != 0 && a6 != 0 && a7 != 0 && a8 != 0 && a9 != 0 )
      return regcam_dispatch_5(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
  }
  return v9;
}


int sceNpCommerce2RegCam_8BBBCF11(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_F8462A1F(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_41(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_ECFD766F(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_11(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_D0320D95(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_59(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_F66BE4F0(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_60(a1, g_regcam_init, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_CA1F31C0(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_3C07C95A(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_42(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_9A5600B9(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_12(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_3D77FBF8(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_13(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_C48D59AE(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_14(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_99D0CFA1(int a1, int a2, int a3)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_15(a1, a2, a3);
  }
  return v2;
}


int sceNpCommerce2RegCam_10FAC26F(int a1, int a2, int a3)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_15(a1, a2, a3);
  }
  return v2;
}


int sceNpCommerce2RegCam_0AA280E7(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_23(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_BA099CBE(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_24(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_90F2F814(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_61(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_290B7E82(int a1, int a2, int a3)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_62(a1, a2, a3);
  return result;
}


int sceNpCommerce2RegCam_0F0D10D7(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_0933A995(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_43(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_D8F02D94(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_16(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_F59AC300(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_63(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_4EAA3049(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_64(a1, a2, a3, a4);
  }
  return result;
}


int sceNpCommerce2RegCam_7C50D970(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_A9925EBC(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_44(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_6E001343(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_17(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_63FAD5E2(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_1(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_6EFF8564(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_65(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_56AB8C1F(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_66(a1, a2, a3, a4);
  }
  return result;
}


int sceNpCommerce2RegCam_46A43687(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_69F468DB(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_45(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_E2AD8DB9(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_18(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_04AF3D66(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_19(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_552B0DF7(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_1(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_F373282E(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_67(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_86966193(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_68(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_427495CA(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_4C191E40(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_69(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_524A8607(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_70(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_A2337870(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_69817297(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_71(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_BCFD9CC5(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_9(a1);
  return result;
}


int sceNpCommerce2RegCam_E256E232(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_4FDB9B84(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_72(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_FA10DDCA(int a1, int a2, int a3, int a4, int a5)
{
  int v5;
  s64 v6;

  v5 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v5 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
    {
      v6 = ((u64)(v6) & 0xFFFFFFFF00000000ULL) | (u32)(a5 == 0);
      v6 = ((u64)(v6) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(a4 == 0) << 32);
      if ( v6 == 0 )
        return regcam_dispatch_6(a1, a2, a3, a4, a5);
    }
  }
  return v5;
}


int sceNpCommerce2RegCam_48D75015(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_00E61B9F(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_46(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_BE4E06A6(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_47(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_F6BBF9FC(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_73(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_1CB4F2B3(int a1, int a2, int a3, int a4)
{
  int v4;

  v4 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v4 = -2141909245;
    if ( a3 != 0 && a4 != 0 )
      return regcam_dispatch_74(a1, a2, a3, a4);
  }
  return v4;
}


int sceNpCommerce2RegCam_4D6A55F2(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_F1711F2E(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_48(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_EB7C8574(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_5(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_51D546D5(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_6(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_BA6DA16C(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_75(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_1BC6B384(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_80(a1);
  return result;
}


int sceNpCommerce2RegCam_748B85C7(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_01B124E9(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_76(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_A4BD8ECD(int a1, int a2, int a3)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a2 != 0 )
    {
      v2 = -2141908950;
      if ( *(u32 *)(a2 + 48) != 0 )
        return regcam_dispatch_77(a1, a2, a3);
    }
  }
  return v2;
}


int sceNpCommerce2RegCam_EDBDE24A(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_B9AA1D92(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_78(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_FB32EF43(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_28(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_46BA74F3(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_1D971360(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_12(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_DA23FE50(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_13(a1, 0);
  return result;
}


int sceNpCommerce2RegCam_AF8404F6(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_9D44ED38(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_20(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_999908AF(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_49(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_3E8C6A5C(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a1 != 0 && a3 != 0 )
      return regcam_tlv_parse_50(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_4F2E8993(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_51(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_1E8D3613(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_79(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_9FCC267C(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_80(a1);
  return result;
}


int sceNpCommerce2RegCam_DE2BB689(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_BD83257C(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_14(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_4937BAA2(int a1)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_dispatch_15(a1);
  return result;
}


int sceNpCommerce2RegCam_0F42FC76(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_CD307E5C(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_3(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_6925F26A(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_81(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_249407CF(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_82(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_BF007C60(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int sceNpCommerce2RegCam_2FC38094(int a1, int a2, int a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != 0 )
      return regcam_tlv_parse_52(a1, a2, a3);
  }
  return v3;
}


int sceNpCommerce2RegCam_28907860(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909245;
    if ( a1 != 0 && a2 != 0 )
      return regcam_tlv_parse_5(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_D6C671FA(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_83(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_F8FBAE67(int a1, int a2)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
  {
    result = -2141909245;
    if ( a2 != 0 )
      return regcam_dispatch_84(a1, a2);
  }
  return result;
}


int sceNpCommerce2RegCam_E68CE3E1(int a1, int a2, int a3, int a4)
{
  int result;

  result = -2141909247;
  if ( g_regcam_init != 0 )
    return regcam_recv_response(a1, a2, a3, a4);
  return result;
}


int regcam_api_helper_15(int a1, int a2)
{
  int v2;

  v2 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v2 = -2141909098;
    if ( (unsigned int)(a2 - 1) < 8 )
      return regcam_set_service_type(a1, a2);
  }
  return v2;
}


int sceNpCommerce2RegCam_5F271C1B(int a1, int a2, u32 *a3)
{
  int v3;

  v3 = -2141909247;
  if ( g_regcam_init != 0 )
  {
    v3 = -2141909245;
    if ( a2 != 0 && a3 != NULL )
    {
      *a3 = 0;
      return regcam_validate_field(a1, a2, a3);
    }
  }
  return v3;
}


