#include "threadman_internal.h"

u32 tman_sched_fn_1(
        int a1,
        float a2,
        float a3,
        float a4,
        float a5,
        float a6,
        float a7,
        float a8,
        float a9)
{
  float v9;
  float v10;
  float v11;
  float v12;
  float v13;
  float v14;
  float v15;
  float v16;
  float v17;
  float v18;
  float v19;
  float v20;
  float v21;
  float v22;
  float v23;
  float v24;
  float v25;
  float v26;
  float v27;
  float v28;
  float v29;
  float v30;
  float v31;
  float v32;
  u32 result;
  int v34;

  _cache(0x18u, (void *volatile)(a1 + 256));
  result = _mfc0(0xCu, 0);
  _mtc0(0xCu, 0, result | 0x20000000);
  v34 = *(char *)(a1 + 36);
  *(float *)(a1 + 272) = v21;
  *(float *)(a1 + 276) = v22;
  *(float *)(a1 + 280) = v23;
  *(float *)(a1 + 284) = v24;
  *(float *)(a1 + 288) = v25;
  *(float *)(a1 + 292) = v26;
  *(float *)(a1 + 296) = v27;
  *(float *)(a1 + 300) = v28;
  *(float *)(a1 + 304) = v29;
  *(float *)(a1 + 308) = v30;
  *(float *)(a1 + 312) = v31;
  *(float *)(a1 + 316) = v32;
  if ( v34 != 1 )
  {
    _cache(0x18u, (void *volatile)(a1 + 192));
    *(float *)(a1 + 192) = v9;
    *(float *)(a1 + 196) = v10;
    *(float *)(a1 + 200) = v11;
    *(float *)(a1 + 204) = v12;
    *(float *)(a1 + 208) = v13;
    *(float *)(a1 + 212) = v14;
    *(float *)(a1 + 216) = v15;
    *(float *)(a1 + 220) = v16;
    *(float *)(a1 + 224) = v17;
    *(float *)(a1 + 228) = v18;
    *(float *)(a1 + 232) = v19;
    *(float *)(a1 + 236) = v20;
    *(float *)(a1 + 240) = a2;
    *(float *)(a1 + 244) = a3;
    *(float *)(a1 + 248) = a4;
    *(float *)(a1 + 252) = a5;
    *(float *)(a1 + 256) = a6;
    *(float *)(a1 + 260) = a7;
    *(float *)(a1 + 264) = a8;
    *(float *)(a1 + 268) = a9;
  }
  _mtc0(0xCu, 0, result);
  tman_ctc0(8, 0);
  return result;
}


int tman_sched_fn_2(int a1, int a2, int a3, int a4, int a5, u32 *a6)
{
  u32 *v11;
  int v12;
  int v13;
  int v14;
  int result;
  int v16;
  int v17;
  u32 *v18;
  u32 *v19;
  int v20;
  int i;
  u32 *v22;
  u32 *v23;
  int v24;

  v11 = (u32 *)TMAN_BSS_U32(0x40);
  v12 = *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  *(u32 *)(TMAN_BSS_U32(0x40) + 120) = 0;
  v13 = a1 + 4 * v12;
  v14 = 0;
  if ( a5 != 0 )
  {
    result = tman_mem_fn_36(a4, tman_sched_fn_13, v11);
    v14 = result;
    if ( result < 0 )
      return result;
    v11[31] = a5;
    v11[30] = result;
  }
  if ( v11[38] != 0 && v11[39] != 0 )
  {
    v11[38] = 0;
    if ( v14 != 0 )
    {
      tman_mem_fn_37(v14, 0);
      v11[31] = 0;
      v11[30] = 0;
    }
    return -2147352149;
  }
  else
  {
    v11[3] = 4;
    v11[22] = a2;
    v11[32] = 0;
    v16 = *(u32 *)(v13 + 8);
    v11[23] = a1;
    *(u32 *)(v13 + 8) = v16 + 1;
    if ( (a3 & 0x100) != 0 )
    {
      v17 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
      v18 = *(u32 **)(v17 + 12);
      v19 = (u32 *)(v17 + 12);
      if ( v18 == (u32 *)(v17 + 12) )
      {
        v20 = *(u32 *)(v17 + 4);
      }
      else
      {
        for ( i = v18[4]; ; i = v18[4] )
        {
          if ( v11[4] < i )
          {
            v20 = *(u32 *)(v17 + 4);
            goto LABEL_15;
          }
          v18 = (u32 *)*v18;
          if ( v18 == v19 )
            break;
        }
        v20 = *(u32 *)(v17 + 4);
      }
LABEL_15:
      v22 = (u32 *)v18[1];
      if ( v20 != 0 && v22 == v19 )
      {
        v18 = (u32 *)*v18;
        v22 = (u32 *)v18[1];
      }
      v11[1] = v22;
      v18[1] = v11;
      v23 = (u32 *)v11[1];
      *v11 = v18;
      *v23 = v11;
    }
    else
    {
      v24 = *(u32 *)(v13 + 16);
      *v11 = v13 + 12;
      v11[1] = v24;
      *(u32 *)(v13 + 16) = v11;
      *(u32 *)v11[1] = v11;
    }
    if ( a6 != NULL )
      ++*a6;
    TMAN_BSS_U32(0x44) = 0;
    tman_sched_fn_19(1, 1);
    return v11[32];
  }
}


u32 *tman_sched_fn_3(int a1, int a2)
{
  u32 *v2;
  int v3;
  u32 *result;

  v2 = (u32 *)TMAN_BSS_U32(0x40);
  v3 = *(u32 *)(a1 + 4);
  *(u32 *)TMAN_BSS_U32(0x40) = a1;
  v2[1] = v3;
  *(u32 *)(a1 + 4) = v2;
  result = (u32 *)v2[1];
  v2[3] = 4;
  v2[22] = a2;
  *result = v2;
  v2[23] = 0;
  v2[32] = 0;
  return result;
}


u32 *tman_sched_fn_4(u32 *a1, int a2)
{
  int v2;
  u32 *v3;
  u32 *v4;
  int v5;
  int i;
  u32 *result;
  u32 *v8;

  v2 = a2 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  v3 = *(u32 **)(v2 + 12);
  v4 = (u32 *)(v2 + 12);
  if ( v3 == (u32 *)(v2 + 12) )
  {
    v5 = *(u32 *)(v2 + 4);
  }
  else
  {
    for ( i = v3[4]; ; i = v3[4] )
    {
      if ( a1[4] < i )
      {
        v5 = *(u32 *)(v2 + 4);
        goto LABEL_9;
      }
      v3 = (u32 *)*v3;
      if ( v3 == v4 )
        break;
    }
    v5 = *(u32 *)(v2 + 4);
  }
LABEL_9:
  result = (u32 *)v3[1];
  if ( v5 != 0 && result == v4 )
  {
    v3 = (u32 *)*v3;
    result = (u32 *)v3[1];
  }
  a1[1] = result;
  v3[1] = a1;
  v8 = (u32 *)a1[1];
  *a1 = v3;
  *v8 = a1;
  return result;
}


int tman_sched_fn_5(int a1, int a2, int a3, int a4, int a5)
{
  return tman_sched_fn_2(a1, a2, a3, a4, a5, NULL);
}


int tman_sched_fn_6(int *a1)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v8;
  int **v9;

  v2 = 0;
  if ( a1[3] != 32 )
  {
    v3 = *a1;
    v4 = a1[1];
    v5 = a1[30];
    a1[38] = 0;
    *(u32 *)(v3 + 4) = v4;
    *(u32 *)a1[1] = v3;
    if ( v5 != 0 )
    {
      tman_mem_fn_37(v5, a1[31]);
      a1[30] = 0;
      a1[31] = 0;
    }
    v6 = 1540658048;
    if ( a1[23] != 0 )
      sceKernelCallUIDObjFunction(a1[23], 1540658048, a1);
    v2 = 0;
    if ( a1[3] == 12 )
    {
      v8 = TMAN_BSS_U32(0x4F4);
      *a1 = (int)&TMAN_BSS_U32(0x4F0);
      a1[1] = v8;
      TMAN_BSS_U32(0x4F4) = (int)a1;
      v9 = (int **)a1[1];
      a1[3] = 8;
      *v9 = a1;
    }
    else
    {
      a1[3] = 2;
      tman_thread_fn_5(a1, v6, &TMAN_BSS_U32(0x4F0), 0);
      return 1;
    }
  }
  return v2;
}


int tman_sched_fn_7(int *a1, int a2, int a3)
{
  int result;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int **v12;

  result = a1[3];
  if ( result != 32 )
  {
    v7 = *a1;
    v8 = a1[1];
    v9 = a1[30];
    a1[38] = 0;
    *(u32 *)(v7 + 4) = v8;
    *(u32 *)a1[1] = v7;
    if ( v9 != 0 )
    {
      result = tman_mem_fn_37(v9, a1[31]);
      a1[30] = 0;
      a1[31] = 0;
    }
    v10 = a1[23];
    if ( v10 != 0 )
      result = sceKernelCallUIDObjFunction(v10, 1540658048, a1);
    if ( a1[3] == 12 )
    {
      v11 = TMAN_BSS_U32(0x4F4);
      *a1 = (int)&TMAN_BSS_U32(0x4F0);
      a1[1] = v11;
      TMAN_BSS_U32(0x4F4) = (int)a1;
      v12 = (int **)a1[1];
      a1[3] = 8;
      *v12 = a1;
    }
    else
    {
      a1[3] = 2;
      if ( a2 != 0 )
      {
        result = tman_thread_fn_5(a1, v7, v9, 12);
        TMAN_BSS_U32(0x44) = 0;
      }
      else
      {
        tman_thread_fn_16(a1, a3);
        return 0;
      }
    }
  }
  return result;
}


int tman_sched_fn_8(int a1, int a2, int a3, int a4)
{
  int *v4;

  v4 = *(int **)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21) + 12);
  v4[32] = a2;
  return tman_sched_fn_7(v4, a3, a4);
}


int tman_sched_fn_9(int a1, int a2)
{
  int v3;
  int *v4;
  int *v5;
  int v6;
  int *v7;

  v3 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  v4 = *(int **)(v3 + 12);
  v5 = (int *)(v3 + 12);
  v6 = 0;
  if ( v4 != (int *)(v3 + 12) )
  {
    v7 = *(int **)(v3 + 12);
    do
    {
      v4[32] = a2;
      ++v6;
      v4 = (int *)*v4;
      tman_sched_fn_6(v7);
      v7 = v4;
    }
    while ( v4 != v5 );
  }
  return v6;
}


int tman_sched_fn_10(u32 *a1)
{
  int v2;
  int result;

  v2 = a1[30];
  a1[38] = 0;
  if ( v2 != 0 )
  {
    tman_mem_fn_37(v2, a1[31]);
    a1[30] = 0;
    a1[31] = 0;
  }
  result = 1540620288;
  if ( a1[23] != 0 )
    return sceKernelCallUIDObjFunction(a1[23], 1540658048, a1);
  return result;
}


int tman_sched_fn_11(int a1, int a2, int a3)
{
  return sceKernelCallUIDObjFunction(a1, -2046940340, a3);
}


int tman_sched_fn_12()
{
  sceKernelCreateUIDtype("WaitQ", 20, g_uid_waitq_funcs, 0, &TMAN_BSS_U32(0x508));
  return 0;
}


int tman_sched_fn_13(int *a1)
{
  int v2;
  u32 *v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int **v13;

  v2 = sceKernelCpuSuspendIntr();
  v3 = (u32 *)a1[31];
  a1[30] = 0;
  if ( v3 != NULL )
    *v3 = 0;
  v4 = a1[3];
  a1[32] = -2147352152;
  v5 = 0;
  a1[31] = 0;
  if ( v4 != 32 )
  {
    v6 = *a1;
    v7 = a1[1];
    v8 = a1[30];
    a1[38] = 0;
    *(u32 *)(v6 + 4) = v7;
    *(u32 *)a1[1] = v6;
    if ( v8 != 0 )
    {
      tman_mem_fn_37(v8, 0);
      a1[30] = 0;
      a1[31] = 0;
    }
    v9 = a1[23];
    if ( v9 != 0 )
      sceKernelCallUIDObjFunction(v9, 1540658048, a1);
    if ( a1[3] == 12 )
    {
      v5 = 0;
      a1[1] = TMAN_BSS_U32(0x4F4);
      TMAN_BSS_U32(0x4F4) = (int)a1;
      v13 = (int **)a1[1];
      *a1 = (int)&TMAN_BSS_U32(0x4F0);
      a1[3] = 8;
      *v13 = a1;
    }
    else
    {
      a1[3] = 2;
      tman_thread_fn_5(a1, 12, v4, v6);
      v5 = 1;
    }
  }
  v10 = a1[23];
  v11 = v5;
  if ( v10 != 0 )
    v11 = v5 + sceKernelCallUIDObjFunction(v10, -2046940340, v4);
  if ( v11 != 0 )
    TMAN_BSS_U32(0x44) = 0;
  return sceKernelCpuResumeIntr(v2);
}


int tman_sched_fn_14(int a1, int a2)
{
  int v4;
  u32 *v5;
  u32 *v6;
  int v7;

  sceKernelCallUIDObjCommonFunction();
  v4 = TMAN_BSS_U32(0x844);
  v5 = (u32 *)(a1 + 4 * *(u8 *)(a2 + 21));
  v5[1] = 0;
  v6 = v5 + 3;
  v5[2] = 0;
  *v5 = 0;
  v7 = *(u32 *)(a1 + 12);
  v5[3] = v5 + 3;
  v6[1] = v6;
  return v7 ^ v4;
}


int tman_sched_fn_15(int a1, int a2, int a3, int a4)
{
  int v8;
  int *v9;
  int *v10;
  int v11;
  int *v12;

  v8 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  v9 = *(int **)(v8 + 12);
  v10 = (int *)(v8 + 12);
  v11 = 0;
  if ( v9 != (int *)(v8 + 12) )
  {
    v12 = *(int **)(v8 + 12);
    do
    {
      v9[32] = -2147352139;
      ++v11;
      v9 = (int *)*v9;
      tman_sched_fn_6(v12);
      v12 = v9;
    }
    while ( v9 != v10 );
  }
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  if ( v11 != 0 )
  {
    TMAN_BSS_U32(0x44) = 0;
    tman_sched_fn_19(0, 0);
  }
  return 0;
}


int tman_sched_fn_16(int a1)
{
  int v1;
  int *v2;
  int *v3;
  int v4;
  int *v5;

  v1 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  v2 = *(int **)(v1 + 12);
  v3 = (int *)(v1 + 12);
  v4 = 0;
  if ( v2 == (int *)(v1 + 12) )
    return v4;
  v5 = *(int **)(v1 + 12);
  do
  {
    v2[32] = -2147352151;
    ++v4;
    v2 = (int *)*v2;
    tman_sched_fn_6(v5);
    v5 = v2;
  }
  while ( v2 != v3 );
  if ( v4 == 0 )
    return v4;
  TMAN_BSS_U32(0x44) = 0;
  tman_sched_fn_19(0, 0);
  return v4;
}


int tman_sched_fn_17()
{
  return 0;
}


int tman_sched_fn_18(int a1, int a2)
{
  --*(u32 *)(a1 + 4 * *(u8 *)(a2 + 21) + 8);
  return 0;
}


void tman_sched_fn_19(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  u32 *v9;
  u32 v10;

  v2 = TMAN_BSS_U32(0x40);
  if ( a1 != 0 )
  {
    v3 = TMAN_BSS_U32(0x40) + 76;
    v4 = 2;
    *(u32 *)(TMAN_BSS_U32(0x40) + 64) = TMAN_BSS_U32(0x40) + 76;
  }
  else
  {
    v3 = TMAN_BSS_U32(0x40) + 72;
    *(u32 *)(TMAN_BSS_U32(0x40) + 64) = TMAN_BSS_U32(0x40) + 72;
    v4 = 1;
  }
  if ( a2 != 0 )
  {
    if ( TMAN_BSS_U32(0x4C) == 0 )
    {
      tman_cfc0(25);
      if ( v3 != 0 )
      {
        v5 = *(u32 *)(v3 + 8);
        *(u32 *)(v3 + 8) = v5 + 1;
        if ( v5 == 0 )
        {
          v9 = *(u32 **)v3;
          *(u32 *)(v3 + 4) = **(u32 **)v3;
          *v9 = 0;
          _sync();
        }
      }
      if ( SaveThreadContext(*(u32 *)(v2 + 28)) == 0 )
      {
        v8 = tman_thread_fn_6(*(u32 *)(v2 + 28), v4);
        InterruptManagerForKernel_551097F2(v8);
      }
      *(u32 *)(v2 + 152) = 0;
      tman_cfc0(25);
      if ( v6 != 0 )
      {
        v7 = *(u32 *)(v6 + 8);
        if ( v7 != 0 )
        {
          *(u32 *)(v6 + 8) = v7 - 1;
          if ( v7 == 1 )
          {
            **(u32 **)v6 = *(u32 *)(v6 + 4);
            _sync();
          }
        }
      }
    }
  }
  else
  {
    v10 = _mfc0(0xDu, 0);
    _mtc0(0xDu, 0, v10 | 0x200);
    sceKernelEnableIntr(65);
  }
}


int sceKernelRegisterThreadEventHandler(int a1, int a2, int a3, int a4, int a5)
{
  int v5;
  int v6;
  int v7;
  int v9;
  int v13;
  int v14;
  s64 v16;
  int v17;
  int v18;
  int v19;
  int v20;
  u32 *v21;
  int v22;
  int ModuleGPByAddressForKernel;
  int v24;
  void (*v25)(int, u32 *, int, int, int, int, int, int);
  int v26;
  int v27;
  int v28;

  v7 = v5 << 11;
  v9 = (v5 << 11) & a1;
  v13 = a2;
  v14 = v5 << 11;
  if ( v9 < 0 || (v7 & a4) < 0 || v7 >= 0 && a4 >= 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a2 != 0 || *(u32 *)(TMAN_BSS_U32(0x40) + 136) != 0 )
    return -2147352476;
  if ( (a3 & 0xFFFFFFF0) != 0 )
    return -2147352170;
  v28 = sceKernelCpuSuspendIntr();
  v16 = ((u64)(v16) & 0xFFFFFFFF00000000ULL) | (u32)(v13 == 0xFFFFFFF0);
  v16 = ((u64)(v16) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v13 == 0xFFFFFFF8) << 32);
  if ( v16 == 0 && v13 != -1 )
  {
    if ( v13 == 0 )
    {
      if ( a3 != 4 )
      {
        sceKernelCpuResumeIntr(v28);
        return -2147352175;
      }
      v13 = *(u32 *)(TMAN_BSS_U32(0x40) + 8);
    }
    if ( sceKernelGetUIDcontrolBlockWithType(v13, TMAN_BSS_U32(0x50C), &v26) == 0 )
    {
      v17 = *(u16 *)(v26 + 22) & 2;
      if ( v14 >= 0 )
        v17 = 2;
      if ( v17 == 0 )
      {
        v18 = v28;
LABEL_29:
        sceKernelCpuResumeIntr(v18);
        return -2147352367;
      }
      v19 = v26 + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
      if ( v14 < 0 )
      {
        v18 = v28;
        if ( (*(u32 *)(TMAN_BSS_U32(0x40) + 20) ^ 0x80000000) >> 28 != (*(u32 *)(v19 + 20) ^ 0x80000000) >> 28 )
          goto LABEL_29;
      }
      goto LABEL_22;
    }
LABEL_30:
    sceKernelCpuResumeIntr(v28);
    return -2147352168;
  }
  if ( v14 < 0 )
  {
    v19 = v13;
    if ( v13 != -16 )
      goto LABEL_30;
  }
  else
  {
    v19 = v13;
  }
LABEL_22:
  v20 = sceKernelCreateUID(TMAN_BSS_U32(0x534), a1, (u8)(v14 >> 31), &v27);
  if ( v20 != 0 )
  {
    sceKernelCpuResumeIntr(v28);
    return v20;
  }
  else
  {
    v21 = (u32 *)(v27 + 4 * *(u8 *)(TMAN_BSS_U32(0x534) + 21));
    v22 = *(u32 *)(v27 + 12) ^ TMAN_BSS_U32(0x844);
    v21[5] = v19;
    v21[6] = (unsigned int)v14 >> 31;
    v21[2] = v22;
    v21[7] = a3;
    v21[8] = a4;
    v21[9] = a5;
    ModuleGPByAddressForKernel = sceKernelGetModuleGPByAddressForKernel(a4);
    v21[10] = ModuleGPByAddressForKernel;
    if ( ModuleGPByAddressForKernel == 0 )
      v21[10] = v6;
    v24 = TMAN_BSS_U32(0x504);
    v25 = (void (*)(int, u32 *, int, int, int, int, int, int))TMAN_BSS_U32(0x48);
    *v21 = &TMAN_BSS_U32(0x500);
    v21[1] = v24;
    TMAN_BSS_U32(0x504) = (int)v21;
    *(u32 *)v21[1] = v21;
    if ( v25 != NULL )
      v25(45, v21, 5, v13, a3, a4, a5, v22);
    sceKernelCpuResumeIntr(v28);
    return v22;
  }
}


int sceKernelReleaseThreadEventHandler(int a1)
{
  int v1;
  int v2;
  int v3;
  int v5;
  int v6;
  int v7;
  int v8;
  u32 v9[4];

  v2 = v1;
  v3 = v1 << 11;
  tman_cfc0(13);
  if ( a1 != 0 || *(u32 *)(TMAN_BSS_U32(0x40) + 136) != 0 )
    return -2147352476;
  v5 = sceKernelCpuSuspendIntr();
  v6 = v5;
  if ( v5 != 0 && TMAN_BSS_U32(0x4C) == 0 )
  {
    if ( sceKernelGetUIDcontrolBlockWithType(0, TMAN_BSS_U32(0x534), v9) != 0 )
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352160;
    }
    else
    {
      v7 = *(u16 *)(v9[0] + 22) & 0x10;
      if ( v3 >= 0 )
        v7 = 16;
      if ( v7 != 0 )
      {
        if ( TMAN_BSS_U32(0x48) != 0 )
          ((void (*)(int, int, int, u32))TMAN_BSS_U32(0x48))(46, v2, 1, 0);
        v8 = sceKernelDeleteUID(0);
        sceKernelCpuResumeIntr(v6);
        return v8;
      }
      else
      {
        sceKernelCpuResumeIntr(v6);
        return -2147352367;
      }
    }
  }
  else
  {
    sceKernelCpuResumeIntr(v5);
    return -2147352153;
  }
}


int tman_sched_fn_20(int a1, int a2, int a3)
{
  int v5;
  int v6;
  int v7; // dc
  int v8;
  int *v9;
  unsigned int v10;
  int v11;
  int v12;
  int v13;
  void (*v15)(int, int, int, int, int, u32, u32);
  int v16;
  int v17;
  int v18;
  int **v19;
  int v21;

  v5 = TMAN_BSS_U32(0x40);
  v6 = TMAN_BSS_U32(0x500);
  v21 = 0;
  v7 = TMAN_BSS_U32(0x500) == (u32)&TMAN_BSS_U32(0x500);
  *(u32 *)(TMAN_BSS_U32(0x40) + 136) |= a1;
  if ( !v7 )
  {
    while ( 1 )
    {
      v8 = v6 - 4 * *(u8 *)(TMAN_BSS_U32(0x534) + 21);
      if ( (a1 & *(u32 *)(v6 + 28)) == 0 )
        goto LABEL_14;
      if ( *(u32 *)(v6 + 16) == 0 )
        break;
      v9 = *(int **)v6;
LABEL_15:
      v6 = (int)v9;
      if ( v9 == &TMAN_BSS_U32(0x500) )
        goto LABEL_16;
    }
    v10 = 0x20000u;
    v11 = *(u32 *)(TMAN_BSS_U32(0x40) + 20);
    if ( v11 < 0 )
    {
      if ( (v11 ^ 0x80000000) >> 28 != (*(u32 *)(a2 + 20) ^ 0x80000000) >> 28 )
      {
        v9 = *(int **)v6;
        goto LABEL_15;
      }
      v12 = *(u32 *)(v6 + 20);
    }
    else
    {
      v12 = *(u32 *)(v6 + 20);
    }
    if ( v12 == -1 )
    {
      v13 = *(u32 *)(v6 + 12);
LABEL_21:
      v15 = (void (*)(int, int, int, int, int, u32, u32))TMAN_BSS_U32(0x48);
      *(u32 *)(v5 + 140) = v8;
      *(u32 *)(v6 + 12) = v13 + 1;
      if ( v15 != NULL )
        v15(8, v6, 4, *(u32 *)(v8 + 12) ^ TMAN_BSS_U32(0x844), a1, *(u32 *)(v6 + 36), *(u32 *)(v6 + 32));
      v16 = tman_sched_fn_28(
              *(u32 *)(v6 + 24),
              *(u32 *)(v6 + 32),
              a1,
              *(u32 *)(a2 + 8),
              *(u32 *)(v6 + 36),
              0,
              *(u32 *)(v6 + 40),
              v10);
      v17 = *(u32 *)(v6 + 12);
      v18 = *(u32 *)(v6 + 16);
      *(u32 *)(v5 + 140) = 0;
      *(u32 *)(v6 + 12) = v17 - 1;
      v9 = *(int **)v6;
      if ( v18 != 0 && v17 == 1 )
      {
        v9[1] = *(u32 *)(v6 + 4);
        v19 = *(int ***)(v6 + 4);
        *(u32 *)(v6 + 4) = v6;
        *v19 = v9;
        *(u32 *)v6 = v6;
        v21 += tman_sched_fn_9(v8, 0);
      }
      else if ( v18 == 0 && v16 != 0 )
      {
        sceKernelDeleteUID(*(u32 *)(v6 + 8));
      }
      goto LABEL_15;
    }
    if ( *(int *)(a2 + 20) < 0 )
    {
      if ( v12 == -16 && v11 < 0 )
      {
        v13 = *(u32 *)(v6 + 12);
        goto LABEL_21;
      }
    }
    else if ( v12 == -8 )
    {
      v13 = *(u32 *)(v6 + 12);
      goto LABEL_21;
    }
    if ( v12 == a2 )
    {
      v13 = *(u32 *)(v6 + 12);
      goto LABEL_21;
    }
LABEL_14:
    v9 = *(int **)v6;
    goto LABEL_15;
  }
LABEL_16:
  if ( TMAN_BSS_U32(0x4C) != 0 )
    tman_thread_fn_10(1, 0, a3);
  if ( v21 != 0 )
  {
    TMAN_BSS_U32(0x44) = 0;
    tman_sched_fn_19(0, a3);
  }
  *(u32 *)(v5 + 136) &= ~a1;
  return 0;
}


int sceKernelReferThreadEventHandlerStatus(int a1, unsigned int *a2)
{
  int v2;
  int v4;
  int v5;
  int UIDcontrolBlockWithType;
  int v8;
  int v9;
  int v10;
  int *v11;
  u32 *v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  unsigned int v18;
  u32 v20[4];

  v4 = ((unsigned int)(a2 + 13) | (unsigned int)a2) & (v2 << 11);
  v5 = v2 << 11;
  if ( v4 < 0 )
    return -2147352365;
  v8 = sceKernelCpuSuspendIntr();
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x534), v20);
  v9 = v20[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v8);
    return -2147352160;
  }
  else
  {
    v10 = *(u16 *)(v20[0] + 22) & 1;
    if ( v5 >= 0 )
      v10 = 1;
    if ( v10 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != 0 )
      {
        ((void (*)(int, int *, int, int, unsigned int *))TMAN_BSS_U32(0x48))(47, &TMAN_BSS_U32(0x40), 2, a1, a2);
        v9 = v20[0];
      }
      v11 = (int *)&TMAN_BSS_U8(0x6E0);
      v12 = (u32 *)(v9 + 4 * *(u8 *)(TMAN_BSS_U32(0x534) + 21));
      do
      {
        ++v11;
        v11[12] = 0;
      }
      while ( v11 != &TMAN_BSS_U32(0x714) );
      v13 = *(u32 *)(v9 + 16);
      TMAN_BSS_U32(0x714) = 52;
      if ( v13 != 0 )
        strncpy(&TMAN_BSS_U32(0x718), v13, 31);
      v14 = v12[5];
      if ( v14 == -8 )
      {
        TMAN_BSS_U32(0x738) = -8;
      }
      else
      {
        v15 = -16;
        if ( v14 >= -7 )
          v15 = -1;
        if ( v14 == v15 )
          TMAN_BSS_U32(0x738) = v12[5];
        else
          TMAN_BSS_U32(0x738) = *(u32 *)(v14 + 8);
      }
      v16 = v12[7];
      v17 = v12[8];
      v18 = *a2;
      TMAN_BSS_U32(0x744) = v12[9];
      TMAN_BSS_U32(0x73C) = v16;
      if ( TMAN_BSS_U32(0x714) < v18 )
        v18 = TMAN_BSS_U32(0x714);
      TMAN_BSS_U32(0x740) = v17;
      memcpy(a2, &TMAN_BSS_U32(0x714), v18);
      sceKernelCpuResumeIntr(v8);
      return 0;
    }
    else
    {
      sceKernelCpuResumeIntr(v8);
      return -2147352367;
    }
  }
}


int sceKernelCreateCallback(int a1, int a2, int a3)
{
  int v3;
  int v4;
  int v5;
  int v7;
  int v9;
  int v11;
  int v13;
  int UID;
  u32 *v15;
  int v16;
  int ModuleGPByAddressForKernel;
  int v18;
  void (*v19)(int, u32 *, int, int, int, int);
  u32 v20[4];

  v5 = v3 << 11;
  v7 = (v3 << 11) & a1;
  v9 = v3 << 11;
  v11 = TMAN_BSS_U32(0x40);
  if ( v7 < 0 || (v5 & a2) < 0 || v5 >= 0 && a2 >= 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a2 != 0 )
    return -2147352476;
  v13 = sceKernelCpuSuspendIntr();
  UID = sceKernelCreateUID(TMAN_BSS_U32(0x538), a1, (u8)(v9 >> 31), v20);
  if ( UID != 0 )
  {
    sceKernelCpuResumeIntr(v13);
    return UID;
  }
  else
  {
    v15 = (u32 *)(v20[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x538) + 21));
    v16 = *(u32 *)(v20[0] + 12) ^ TMAN_BSS_U32(0x844);
    v15[8] = (unsigned int)v9 >> 31;
    v15[2] = v16;
    v15[5] = v11;
    v15[6] = 0;
    v15[7] = 0;
    v15[9] = a2;
    v15[10] = a3;
    ModuleGPByAddressForKernel = sceKernelGetModuleGPByAddressForKernel(a2);
    v15[11] = ModuleGPByAddressForKernel;
    if ( ModuleGPByAddressForKernel == 0 )
      v15[11] = v4;
    v18 = *(u32 *)(v11 + 148);
    v19 = (void (*)(int, u32 *, int, int, int, int))TMAN_BSS_U32(0x48);
    *v15 = v11 + 144;
    v15[1] = v18;
    *(u32 *)(v11 + 148) = v15;
    *(u32 *)v15[1] = v15;
    if ( v19 != NULL )
      v19(156, v15, 3, a2, a3, v16);
    sceKernelCpuResumeIntr(v13);
    return v16;
  }
}


int sceKernelDeleteCallback(int a1)
{
  int v1;
  int v2;
  int v3;
  int v4;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[4];

  tman_cfc0(13);
  v3 = v2;
  v4 = v2 << 11;
  if ( v1 != 0 || *(u32 *)(TMAN_BSS_U32(0x40) + 164) != 0 )
    return -2147352476;
  v7 = sceKernelCpuSuspendIntr();
  v8 = v7;
  if ( v7 != 0 && TMAN_BSS_U32(0x4C) == 0 )
  {
    if ( sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x538), v11) != 0 )
    {
      sceKernelCpuResumeIntr(v8);
      return -2147352159;
    }
    else
    {
      v9 = *(u16 *)(v11[0] + 22) & 0x10;
      if ( v4 >= 0 )
        v9 = 16;
      if ( v9 != 0 )
      {
        if ( TMAN_BSS_U32(0x48) != 0 )
          ((void (*)(int, int, int, int))TMAN_BSS_U32(0x48))(157, v3, 1, a1);
        v10 = sceKernelDeleteUID(a1);
        sceKernelCpuResumeIntr(v8);
        return v10;
      }
      else
      {
        sceKernelCpuResumeIntr(v8);
        return -2147352367;
      }
    }
  }
  else
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352153;
  }
}


int sceKernelNotifyCallback(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v7;
  int v8;
  int v9;
  u32 *v10;
  int *v11;
  int v12;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  u32 v19[4];

  v4 = v3 << 11;
  tman_cfc0(13);
  v7 = sceKernelCpuSuspendIntr();
  if ( sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x538), v19) != 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352159;
  }
  else
  {
    v8 = *(u16 *)(v19[0] + 22) & 2;
    if ( v4 >= 0 )
      v8 = 2;
    if ( v8 != 0 )
    {
      v9 = *(u8 *)(TMAN_BSS_U32(0x538) + 21);
      v10 = (u32 *)(v19[0] + 4 * v9);
      if ( TMAN_BSS_U32(0x48) != 0 )
        ((void (*)(int, int, int, int, int))TMAN_BSS_U32(0x48))(158, v19[0] + 4 * v9, 2, a1, a2);
      v11 = (int *)v10[5];
      v11[39] = 1;
      v12 = v10[6];
      v10[7] = a2;
      v10[6] = v12 + 1;
      if ( v11[38] != 0 && (v11[3] == 4 || v11[3] == 0xC) )
      {
        v11[32] = -2147352149;
        v14 = tman_sched_fn_6(v11);
        v17 = v11[23];
        v18 = v14;
        if ( v17 != 0 )
          v18 = v14 + tman_sched_fn_11(v17, v15, v16);
        if ( v18 != 0 )
        {
          TMAN_BSS_U32(0x44) = 0;
          if ( v2 == 0 )
            tman_sched_fn_19(0, v7);
        }
      }
      sceKernelCpuResumeIntr(v7);
      return 0;
    }
    else
    {
      sceKernelCpuResumeIntr(v7);
      return -2147352367;
    }
  }
}


int sceKernelCancelCallback(int a1)
{
  int v1;
  int v2;
  int v4;
  int v5;
  int v6;
  u32 v8[4];

  v4 = sceKernelCpuSuspendIntr();
  v2 = v1 << 11;
  if ( sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x538), v8) != 0 )
  {
    sceKernelCpuResumeIntr(v4);
    return -2147352159;
  }
  else
  {
    v5 = *(u16 *)(v8[0] + 22) & 8;
    if ( v2 >= 0 )
      v5 = 8;
    if ( v5 != 0 )
    {
      v6 = v8[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x538) + 21);
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((int (*)())TMAN_BSS_U32(0x48))(159, &TMAN_BSS_U32(0x40), 1, a1);
      *(u32 *)(v6 + 28) = 0;
      *(u32 *)(v6 + 24) = 0;
      sceKernelCpuResumeIntr(v4);
      return 0;
    }
    else
    {
      sceKernelCpuResumeIntr(v4);
      return -2147352367;
    }
  }
}


int sceKernelGetCallbackCount(int a1)
{
  int v1;
  int v2;
  int v4;
  int v5;
  int v6;
  u32 v8[4];

  v4 = sceKernelCpuSuspendIntr();
  v2 = v1 << 11;
  if ( sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x538), v8) != 0 )
  {
    sceKernelCpuResumeIntr(v4);
    return -2147352159;
  }
  else
  {
    v5 = *(u16 *)(v8[0] + 22) & 1;
    if ( v2 >= 0 )
      v5 = 1;
    if ( v5 != 0 )
    {
      v6 = *(u32 *)(v8[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x538) + 21) + 24);
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((int (*)())TMAN_BSS_U32(0x48))(160, &TMAN_BSS_U32(0x40), 2, a1);
      sceKernelCpuResumeIntr(v4);
      return v6;
    }
    else
    {
      sceKernelCpuResumeIntr(v4);
      return -2147352367;
    }
  }
}


int sceKernelCheckCallback()
{
  int v0;
  int v1;
  int v2;
  int v4;
  int v5;

  v1 = false;
  v2 = TMAN_BSS_U32(0x40);
  tman_cfc0(13);
  if ( 0x20000 != 0 || *(u32 *)(TMAN_BSS_U32(0x40) + 164) != 0 )
    return -2147352476;
  v4 = sceKernelCpuSuspendIntr(0, -2147352576);
  v5 = v4;
  if ( v4 != 0 && TMAN_BSS_U32(0x4C) == 0 )
  {
    if ( *(u32 *)(v2 + 156) != 0 )
      v1 = tman_sched_fn_21(v4) != 0;
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((int (*)())TMAN_BSS_U32(0x48))(162, v0, 1, v1);
    sceKernelCpuResumeIntr(v5);
    return v1;
  }
  else
  {
    sceKernelCpuResumeIntr(v4);
    return -2147352153;
  }
}


int tman_sched_fn_21(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
  int v8;
  int v9;
  int v10;
  u32 *v11;
  int v12; // dc
  int v13;
  int v14;
  u32 *v15;
  int v16;
  void (*v17)(int, u32 *, int, int, int, int, u32, u32);
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  u32 *v24;
  int v26;

  v8 = 0;
  v9 = TMAN_BSS_U32(0x40);
  v10 = TMAN_BSS_U32(0x40) + 144;
  v11 = *(u32 **)(TMAN_BSS_U32(0x40) + 144);
  v26 = 0;
  v12 = v11 == (u32 *)(TMAN_BSS_U32(0x40) + 144);
  *(u32 *)(TMAN_BSS_U32(0x40) + 156) = 0;
  if ( !v12 )
  {
    do
    {
      v13 = v11[6];
      v14 = (int)&v11[-*(u8 *)(TMAN_BSS_U32(0x538) + 21)];
      if ( v13 != 0 )
      {
        if ( v11[4] != 0 )
        {
          v15 = (u32 *)*v11;
        }
        else
        {
          v11[6] = 0;
          v16 = v11[7];
          v11[7] = 0;
          v17 = (void (*)(int, u32 *, int, int, int, int, u32, u32))TMAN_BSS_U32(0x48);
          v18 = *(u32 *)(v9 + 164);
          ++*(u32 *)(v9 + 160);
          v19 = v11[3];
          *(u32 *)(v9 + 164) = v14;
          v11[3] = v19 + 1;
          if ( v17 != NULL )
            v17(7, v11, 5, *(u32 *)(v14 + 12) ^ TMAN_BSS_U32(0x844), v13, v16, v11[10], v11[9]);
          v20 = tman_sched_fn_28(v11[8], v11[9], v13, v16, v11[10], 0, v11[11], a8);
          v21 = v11[3];
          *(u32 *)(v9 + 164) = v18;
          v11[3] = v21 - 1;
          v15 = (u32 *)*v11;
          a8 = *(u32 *)(v9 + 160) - 1;
          *(u32 *)(v9 + 160) = a8;
          v22 = v11[4];
          ++v8;
          if ( v22 == 0 || v11[3] != 0 )
          {
            if ( v22 == 0 && v20 != 0 )
              sceKernelDeleteUID(v11[2]);
          }
          else
          {
            v15[1] = v11[1];
            v24 = (u32 *)v11[1];
            v11[1] = v11;
            *v24 = v15;
            *v11 = v11;
            v26 += tman_sched_fn_9(v14, 0);
          }
        }
      }
      else
      {
        v15 = (u32 *)*v11;
      }
      v11 = v15;
    }
    while ( v15 != (u32 *)v10 );
  }
  if ( TMAN_BSS_U32(0x4C) != 0 )
    tman_thread_fn_10(1, 0, a1);
  if ( v26 != 0 )
  {
    TMAN_BSS_U32(0x44) = 0;
    tman_sched_fn_19(0, a1);
  }
  return v8;
}


int sceKernelReferCallbackStatus(int a1, unsigned int *a2)
{
  int v2;
  int v4;
  int v5;
  int v6;
  int UIDcontrolBlockWithType;
  int v9;
  int v10;
  int v11;
  int *v12;
  u32 *v13;
  int v14;
  int v15;
  int v16;
  int v17;
  unsigned int v18;
  int v19;
  u32 v21[4];

  v4 = (unsigned int)(a2 + 14) | (unsigned int)a2;
  v5 = v4 & (v2 << 11);
  v6 = v2 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v9 = sceKernelCpuSuspendIntr(a1, v4);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x538), v21);
  v10 = v21[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352159;
  }
  else
  {
    v11 = *(u16 *)(v21[0] + 22) & 1;
    if ( v6 >= 0 )
      v11 = 1;
    if ( v11 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int *, int, int, unsigned int *))TMAN_BSS_U32(0x48))(163, &TMAN_BSS_U32(0x40), 2, a1, a2);
        v10 = v21[0];
      }
      v12 = (int *)&TMAN_BSS_U8(0x6DC);
      v13 = (u32 *)(v10 + 4 * *(u8 *)(TMAN_BSS_U32(0x538) + 21));
      do
      {
        ++v12;
        v12[13] = 0;
      }
      while ( v12 != &TMAN_BSS_U32(0x714) );
      v14 = *(u32 *)(v10 + 16);
      TMAN_BSS_U32(0x714) = 56;
      if ( v14 != 0 )
        strncpy(&TMAN_BSS_U32(0x718), v14, 31);
      v15 = v13[9];
      v16 = v13[10];
      TMAN_BSS_U32(0x738) = *(u32 *)(v13[5] + 8);
      v17 = v13[6];
      v18 = *a2;
      TMAN_BSS_U32(0x73C) = v15;
      TMAN_BSS_U32(0x744) = v17;
      if ( TMAN_BSS_U32(0x714) < v18 )
        v18 = TMAN_BSS_U32(0x714);
      v19 = v13[7];
      TMAN_BSS_U32(0x740) = v16;
      TMAN_BSS_U32(0x748) = v19;
      memcpy(a2, &TMAN_BSS_U32(0x714), v18);
      sceKernelCpuResumeIntr(v9);
      return 0;
    }
    else
    {
      sceKernelCpuResumeIntr(v9);
      return -2147352367;
    }
  }
}


int sceKernelReturnFromCallback(int a1, int a2)
{
  sceKernelCpuSuspendIntr(a1, a2);
  *(u32 *)(TMAN_BSS_U32(0x40) + 128) = *(u32 *)(*(u32 *)(TMAN_BSS_U32(0x40) + 36) + 24);
  return tman_sched_fn_27();
}


void tman_sched_fn_22(int a1)
{
  int v1;
  int *v3;
  int v4;
  int v5;
  int v6;
  u32 *v7;

  v1 = *(u32 *)(a1 + 140);
  if ( v1 != 0 )
  {
    v3 = (int *)(v1 + 4 * *(u8 *)(TMAN_BSS_U32(0x534) + 21));
    v4 = v3[3];
    v5 = v3[4];
    v3[3] = v4 - 1;
    if ( v5 != 0 && v4 == 1 )
    {
      v6 = *v3;
      *(u32 *)(v6 + 4) = v3[1];
      v7 = (u32 *)v3[1];
      v3[1] = (int)v3;
      *v7 = v6;
      *v3 = (int)v3;
      tman_sched_fn_9(v1, 0);
      *(u32 *)(a1 + 140) = 0;
    }
    else
    {
      *(u32 *)(a1 + 140) = 0;
    }
    *(u32 *)(a1 + 136) = 0;
  }
}


int tman_sched_fn_23(int a1, int a2)
{
  int v2;
  int *v3;
  int v4;
  int v5;

  v2 = TMAN_BSS_U32(0x500);
  if ( (int *)TMAN_BSS_U32(0x500) == &TMAN_BSS_U32(0x500) )
    return 0;
  while ( 1 )
  {
    v3 = *(int **)v2;
    if ( (a1 & *(u32 *)(v2 + 28)) == 0 || *(u32 *)(v2 + 16) != 0 )
      goto LABEL_10;
    v4 = *(u32 *)(TMAN_BSS_U32(0x40) + 20);
    if ( v4 < 0 )
    {
      if ( (v4 ^ 0x80000000) >> 28 != (*(u32 *)(a2 + 20) ^ 0x80000000) >> 28 )
        goto LABEL_10;
      v5 = *(u32 *)(v2 + 20);
    }
    else
    {
      v5 = *(u32 *)(v2 + 20);
    }
    if ( v5 == -1 )
      return 1;
    if ( *(int *)(a2 + 20) < 0 )
      break;
    if ( v5 == -8 )
      return 1;
LABEL_9:
    if ( v5 == a2 )
      return 1;
LABEL_10:
    v2 = *(u32 *)v2;
    if ( v3 == &TMAN_BSS_U32(0x500) )
      return 0;
  }
  if ( v5 != -16 || v4 >= 0 )
    goto LABEL_9;
  return 1;
}


int tman_sched_fn_24(u32 *a1)
{
  u32 *v1;
  u32 *v2;
  int v4;
  int result;

  v1 = a1 + 36;
  v2 = (u32 *)a1[36];
  if ( v2 != a1 + 36 )
  {
    for ( v2[3] = 0; ; v2[3] = 0 )
    {
      v4 = v2[2];
      v2 = (u32 *)*v2;
      result = sceKernelDeleteUID(v4);
      if ( v2 == v1 )
        break;
    }
  }
  a1[40] = 0;
  a1[39] = 0;
  a1[41] = 0;
  return result;
}


int ThreadManForUser_8672E3D0(int a1, int a2)
{
  int v2;
  int v3;
  int v4;

  v3 = -2147352476;
  tman_cfc0(13);
  if ( a2 == 0 )
  {
    v4 = *(u32 *)(TMAN_BSS_U32(0x40) + 160);
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((void (*)(int, int, int, u32, u32))TMAN_BSS_U32(0x48))(
        161,
        v2,
        2,
        *(u32 *)(TMAN_BSS_U32(0x40) + 8),
        *(u32 *)(TMAN_BSS_U32(0x40) + 160));
    return v4;
  }
  return v3;
}


void tman_sched_fn_25(int a1)
{
  int v1;
  int *v3;
  int v4;
  int v5;
  int v6;
  u32 *v7;

  v1 = *(u32 *)(a1 + 164);
  if ( v1 != 0 )
  {
    v3 = (int *)(v1 + 4 * *(u8 *)(TMAN_BSS_U32(0x538) + 21));
    v4 = v3[3];
    v5 = v3[4];
    v3[3] = v4 - 1;
    if ( v5 != 0 && v4 == 1 )
    {
      v6 = *v3;
      *(u32 *)(v6 + 4) = v3[1];
      v7 = (u32 *)v3[1];
      v3[1] = (int)v3;
      *v7 = v6;
      *v3 = (int)v3;
      tman_sched_fn_9(v1, 0);
      *(u32 *)(a1 + 164) = 0;
    }
    else
    {
      *(u32 *)(a1 + 164) = 0;
    }
  }
}


int tman_sched_fn_26()
{
  sceKernelCreateUIDtypeInherit("WaitQ", "ThreadEventHandler", 48, g_uid_threv_funcs, 0, &TMAN_BSS_U32(0x534));
  sceKernelCreateUIDtypeInherit("WaitQ", "Callback", 48, g_uid_cb_funcs, 0, &TMAN_BSS_U32(0x538));
  return 0;
}


int tman_sched_fn_27()
{
  int v0;
  int v1;
  int v2;
  int v4;

  tman_cfc0(13);
  v1 = TMAN_BSS_U32(0x40);
  if ( v0 != 0 || (v2 = *(u32 *)(TMAN_BSS_U32(0x40) + 132)) == 0 )
    _break(3u, 0x3FFu);
  if ( (*(u32 *)(v2 + 52) & 0x40000000) != 0 )
  {
    v4 = TMAN_BSS_U32(0x40);
    *(u32 *)(TMAN_BSS_U32(0x40) + 20) |= 0x4000u;
    tman_thread_fn_7(v4);
  }
  else
  {
    *(u32 *)(TMAN_BSS_U32(0x40) + 20) &= ~0x4000u;
  }
  InterruptManagerForKernel_551097F2(*(u32 *)(v1 + 132));
  return 0;
}


int tman_sched_fn_28(int a1, int a2, int a3, int a4, int a5, int a6, int a7)
{
  unsigned int v7;
  u32 *v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v21;
  int v22;
  int *v23;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  u8 v30[321];

  v7 = (unsigned int)v30 & 0xFFFFFFC0;
  tman_thread_fn_8((unsigned int)v30 & 0xFFFFFFC0);
  *(u32 *)((char *)0x38 + ((unsigned int)v30 & 0xFFFFFFC0)) = a2;
  *(u32 *)((char *)0x50 + ((unsigned int)v30 & 0xFFFFFFC0)) = a3;
  v15 = (u32 *)TMAN_BSS_U32(0x40);
  *(u32 *)((char *)0x54 + ((unsigned int)v30 & 0xFFFFFFC0)) = a4;
  *(u32 *)((char *)0x58 + ((unsigned int)v30 & 0xFFFFFFC0)) = a5;
  *(u32 *)((char *)0x5C + ((unsigned int)v30 & 0xFFFFFFC0)) = a6;
  *(u32 *)((char *)0xB0 + ((unsigned int)v30 & 0xFFFFFFC0)) = a7;
  v16 = v15[6];
  v27 = v15[10];
  v28 = v15[9];
  v29 = v15[33];
  tman_thread_fn_17();
  v17 = v15[7];
  v15[33] = v17;
  if ( SaveThreadContext(v17) != 0 )
  {
    v19 = v15[33];
    v15[10] = v27;
    v15[9] = v28;
    v15[33] = v29;
    v15[6] = v16;
    v15[7] = v19;
    tman_cfc0(8);
    if ( v18 == v7 )
      tman_ctc0(8, 0);
    tman_ctc0(14, 0);
    tman_ctc0(15, 0);
    return v15[32];
  }
  else
  {
    v15[7] = v7;
    if ( a1 != 0 )
    {
      v21 = v15[9];
      if ( v21 != 0 )
        v22 = *(u32 *)(v21 + 8);
      else
        v22 = v15[50];
      v23 = (int *)(v22 - 64);
      *v23 = (TMAN_BSS_U32(0x81C) << 6) & 0x3FFFFC0 | 0xC;
      v23[1] = 0;
      v23[2] = 0;
      v23[3] = 0;
      _cache(0x1Bu, v23);
      _cache(8u, v23);
      _sync();
      *(u32 *)((char *)0xBC + ((unsigned int)v30 & 0xFFFFFFC0)) = v23;
      v24 = 19;
      v15[10] = *(u32 *)(v15[33] + 180);
    }
    else
    {
      v23 = *(int **)(v15[33] + 180);
      *(u32 *)((char *)0xBC + ((unsigned int)v30 & 0xFFFFFFC0)) = tman_sched_fn_33;
      v24 = 3;
      v15[10] = 0;
    }
    v25 = v15[55];
    *(u32 *)((char *)0x34 + ((unsigned int)v30 & 0xFFFFFFC0)) = v24;
    *(u32 *)((char *)0x24 + ((unsigned int)v30 & 0xFFFFFFC0)) = 3;
    *(u32 *)((char *)0xB8 + ((unsigned int)v30 & 0xFFFFFFC0)) = v23;
    *(u32 *)((char *)0xA8 + ((unsigned int)v30 & 0xFFFFFFC0)) = v25;
    *(u32 *)((char *)0xB4 + ((unsigned int)v30 & 0xFFFFFFC0)) = v23;
    v26 = v15[5];
    v15[6] = 0;
    v15[5] = v26 & 0xFFFFBFFF;
    tman_ctc0(15, 0); tman_ctc0(14, 0);
    InterruptManagerForKernel_551097F2((unsigned int)v30 & 0xFFFFFFC0);
    return 0;
  }
}


int tman_sched_fn_29(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_sched_fn_30(int a1, int a2, int a3, int a4)
{
  int v8;
  int *v9;
  int v10;
  int v11;
  int v12;

  v8 = sceKernelCpuSuspendIntr(a1, a2);
  v9 = (int *)(a1 + 4 * *(u8 *)(a2 + 21));
  if ( v9[4] != 0 )
  {
    sceKernelCpuResumeIntr(v8);
    return -2147352160;
  }
  else
  {
    v10 = v9[3];
    v9[4] = 1;
    if ( v10 <= 0 )
    {
LABEL_5:
      v12 = *v9;
      *(u32 *)(v12 + 4) = v9[1];
      *(u32 *)v9[1] = v12;
      sceKernelCpuResumeIntr(v8);
      sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
      return 0;
    }
    else
    {
      while ( 1 )
      {
        tman_thread_fn_17();
        v11 = tman_sched_fn_5(a1, 10, 0, 0, 0);
        if ( v11 != 0 )
          break;
        if ( v9[3] <= 0 )
          goto LABEL_5;
      }
      sceKernelCpuResumeIntr(v8);
      return v11;
    }
  }
}


int tman_sched_fn_31(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_sched_fn_32(int a1, int a2, int a3, int a4)
{
  int v8;
  int *v9;
  int v10;
  int v11;
  int v12;

  v8 = sceKernelCpuSuspendIntr(a1, a2);
  v9 = (int *)(a1 + 4 * *(u8 *)(a2 + 21));
  if ( v9[4] != 0 )
  {
    sceKernelCpuResumeIntr(v8);
    return -2147352159;
  }
  else
  {
    v10 = v9[3];
    v9[4] = 1;
    if ( v10 <= 0 )
    {
LABEL_5:
      v12 = *v9;
      *(u32 *)(v12 + 4) = v9[1];
      *(u32 *)v9[1] = v12;
      sceKernelCpuResumeIntr(v8);
      sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
      return 0;
    }
    else
    {
      while ( 1 )
      {
        tman_thread_fn_17();
        v11 = tman_sched_fn_5(a1, 11, 0, 0, 0);
        if ( v11 != 0 )
          break;
        if ( v9[3] <= 0 )
          goto LABEL_5;
      }
      sceKernelCpuResumeIntr(v8);
      return v11;
    }
  }
}


int tman_sched_fn_33(int a1, int a2)
{
  int v2;
  int v3;

  v3 = v2;
  sceKernelCpuSuspendIntr(a1, a2);
  *(u32 *)(TMAN_BSS_U32(0x40) + 128) = v3;
  return tman_sched_fn_27();
}


int sceKernelSleepThread(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v6;
  int v7;
  int v8;

  tman_cfc0(13);
  if ( v2 != 0 )
    return -2147352476;
  v3 = sceKernelCpuSuspendIntr(a1, a2);
  v4 = v3;
  if ( v3 != 0 && TMAN_BSS_U32(0x4C) == 0 )
  {
    v6 = 48;
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((void (*)(int, unsigned int, u32))TMAN_BSS_U32(0x48))(48, 0x20000u, 0);
    tman_thread_fn_17(v6);
    v7 = TMAN_BSS_U32(0x40);
    v8 = *(u32 *)(TMAN_BSS_U32(0x40) + 80);
    *(u32 *)(TMAN_BSS_U32(0x40) + 128) = 0;
    if ( v8 != 0 )
    {
      *(u32 *)(v7 + 80) = v8 - 1;
    }
    else
    {
      tman_sched_fn_3((int)&TMAN_BSS_U32(0x4D8), 1);
      TMAN_BSS_U32(0x44) = 0;
      tman_sched_fn_19(1, 1);
    }
    sceKernelCpuResumeIntr(v4);
    return *(u32 *)(v7 + 128);
  }
  else
  {
    sceKernelCpuResumeIntr(v3);
    return -2147352153;
  }
}


int sceKernelSleepThreadCB(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  u32 *v14;
  int i;
  int v16;

  tman_cfc0(13);
  if ( v2 != 0 )
    return -2147352476;
  v4 = sceKernelCpuSuspendIntr(a1, a2);
  v5 = v4;
  if ( v4 != 0 && TMAN_BSS_U32(0x4C) == 0 )
  {
    v7 = 49;
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((void (*)(int, int, u32))TMAN_BSS_U32(0x48))(49, v3, 0);
    tman_thread_fn_17(v7);
    v14 = (u32 *)TMAN_BSS_U32(0x40);
    for ( i = *(u32 *)(TMAN_BSS_U32(0x40) + 156); ; i = v14[39] )
    {
      if ( i != 0 )
      {
        do
        {
          tman_sched_fn_21(v5, v8, i, v9, v10, v11, v12, v13);
          v9 = v14[39];
        }
        while ( v9 != 0 );
        v16 = v14[20];
      }
      else
      {
        v16 = v14[20];
      }
      v14[32] = 0;
      if ( v16 != 0 )
      {
        v10 = v16 - 1;
        v14[20] = v16 - 1;
      }
      else
      {
        tman_sched_fn_3((int)&TMAN_BSS_U32(0x4D8), 1);
        v14[38] = 1;
        TMAN_BSS_U32(0x44) = 0;
        tman_sched_fn_19(1, 1);
      }
      v11 = v14[32];
      if ( v11 != -2147352149 )
        break;
    }
    sceKernelCpuResumeIntr(v5);
    return v14[32];
  }
  else
  {
    sceKernelCpuResumeIntr(v4);
    return -2147352153;
  }
}


int sceKernelWakeupThread(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v6;
  int v7;
  int *v8;
  int v9;
  s64 v10;
  int v11;
  u32 v13[4];

  v4 = v3 << 11;
  if ( a1 == 0 )
    return -2147352169;
  v6 = sceKernelCpuSuspendIntr(a1, a2);
  tman_cfc0(13);
  if ( sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x50C), v13) != 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352168;
  }
  v7 = *(u16 *)(v13[0] + 22) & 2;
  if ( v4 >= 0 )
    v7 = 2;
  if ( v7 == 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352367;
  }
  v8 = (int *)(v13[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21));
  if ( v2 == 0 && v8 == (int *)TMAN_BSS_U32(0x40) )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352169;
  }
  v9 = v8[3];
  v10 = ((u64)(v10) & 0xFFFFFFFF00000000ULL) | (u32)(v9 == 0x10);
  v10 = ((u64)(v10) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v9 == 0x20) << 32);
  if ( v10 == 0 )
  {
    if ( TMAN_BSS_U32(0x48) != NULL )
    {
      ((int (*)())TMAN_BSS_U32(0x48))(50, v8, 1, a1);
      v9 = v8[3];
    }
    if ( v9 == 4 || v9 == 0xC )
    {
      if ( v8[22] == 1 )
      {
        tman_sched_fn_7(v8, v2, v6);
        goto LABEL_16;
      }
      v11 = v8[20];
    }
    else
    {
      v11 = v8[20];
    }
    v8[20] = v11 + 1;
LABEL_16:
    sceKernelCpuResumeIntr(v6);
    return 0;
  }
  sceKernelCpuResumeIntr(v6);
  return -2147352158;
}


int sceKernelCancelWakeupThread(int a1, int a2)
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
  u32 v12[4];

  tman_cfc0(13);
  v4 = v3 << 11;
  v5 = a1;
  if ( a1 == 0 )
  {
    if ( v2 != 0 )
      return -2147352169;
    a1 = TMAN_BSS_U32(0x40);
    v5 = *(u32 *)(TMAN_BSS_U32(0x40) + 8);
  }
  v6 = sceKernelCpuSuspendIntr(a1, a2);
  if ( sceKernelGetUIDcontrolBlockWithType(v5, TMAN_BSS_U32(0x50C), v12) != 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352168;
  }
  else
  {
    v7 = *(u16 *)(v12[0] + 22) & 8;
    if ( v4 >= 0 )
      v7 = 8;
    if ( v7 != 0 )
    {
      v8 = *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
      v9 = v12[0] + 4 * v8;
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((int (*)())TMAN_BSS_U32(0x48))(51, v12[0] + 4 * v8, 1, v5);
      v10 = *(u32 *)(v9 + 80);
      *(u32 *)(v9 + 80) = 0;
      sceKernelCpuResumeIntr(v6);
      return v10;
    }
    else
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352367;
    }
  }
}


int sceKernelDonateWakeupThread(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v6;
  int v7;
  int v9;
  int v10;
  int v11;
  int *v12;
  int v13;
  int v14;
  u32 v15[4];

  v4 = v3 << 11;
  if ( a1 == 0 )
    return -2147352169;
  tman_cfc0(13);
  if ( v2 != 0 )
    return -2147352476;
  v6 = sceKernelCpuSuspendIntr(a1, a2);
  v7 = v6;
  if ( v6 == 0 || TMAN_BSS_U32(0x4C) != 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352153;
  }
  tman_thread_fn_17(0);
  if ( sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x50C), v15) != 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352168;
  }
  v9 = *(u16 *)(v15[0] + 22) & 2;
  if ( v4 >= 0 )
    v9 = 2;
  v10 = TMAN_BSS_U32(0x40);
  if ( v9 == 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352367;
  }
  v11 = *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
  v12 = (int *)(v15[0] + 4 * v11);
  if ( v12 == (int *)TMAN_BSS_U32(0x40) )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352169;
  }
  if ( v12[3] == 0x10 || v12[3] == 0x20 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352158;
  }
  else
  {
    v13 = *(u32 *)(TMAN_BSS_U32(0x40) + 80);
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((void (*)(int, int, int, int, u32))TMAN_BSS_U32(0x48))(
        52,
        v15[0] + 4 * v11,
        2,
        a1,
        *(u32 *)(TMAN_BSS_U32(0x40) + 80));
    *(u32 *)(v10 + 80) = 0;
    *(u32 *)(v10 + 128) = 0;
    tman_sched_fn_3((int)&TMAN_BSS_U32(0x4D8), 1);
    v14 = v12[3];
    v12[20] += v13;
    if ( (v14 == 4 || v14 == 0xC) && v12[22] == 1 )
      tman_sched_fn_6(v12);
    TMAN_BSS_U32(0x44) = 0;
    tman_sched_fn_19(1, 1);
    sceKernelCpuResumeIntr(v7);
    return *(u32 *)(v10 + 128);
  }
}


int sceKernelSuspendThread(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v6;
  int v7;
  u32 *v8;
  int v9;
  u32 *v11;
  u32 v12[4];

  v4 = v3 << 11;
  if ( a1 == 0 )
    return -2147352169;
  v6 = sceKernelCpuSuspendIntr(a1, a2);
  tman_cfc0(13);
  if ( sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x50C), v12) != 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352168;
  }
  v7 = *(u16 *)(v12[0] + 22) & 2;
  if ( v4 >= 0 )
    v7 = 2;
  if ( v7 == 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352367;
  }
  v8 = (u32 *)(v12[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21));
  if ( v2 == 0 && v8 == (u32 *)TMAN_BSS_U32(0x40) )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352169;
  }
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(53, v8, 1, a1);
  switch ( v8[3] )
  {
    case 1:
      if ( TMAN_BSS_U32(0x4C) == 0 )
        goto LABEL_16;
      if ( TMAN_BSS_U32(0x4C) != 3 )
        TMAN_BSS_U32(0x4C) = 2;
      v9 = 0;
      break;
    case 2:
LABEL_16:
      if ( v2 != 0 && v8 == (u32 *)TMAN_BSS_U32(0x40) )
        TMAN_BSS_U32(0x44) = 0;
      else
        tman_thread_fn_15(v8, v8[4]);
      v9 = 0;
      v8[1] = TMAN_BSS_U32(0x4F4);
      TMAN_BSS_U32(0x4F4) = (int)v8;
      v11 = (u32 *)v8[1];
      *v8 = &TMAN_BSS_U32(0x4F0);
      v8[3] = 8;
      *v11 = v8;
      break;
    case 4:
      v9 = 0;
      v8[3] = 12;
      break;
    case 8:
    case 0xC:
      v9 = -2147352157;
      break;
    default:
      v9 = -2147352158;
      break;
  }
  sceKernelCpuResumeIntr(v6);
  return v9;
}


int sceKernelResumeThread(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int UIDcontrolBlockWithType;
  int v7;
  int v8;
  int v9;
  int *v10;
  int v11;
  int v12;
  int v13;
  int v15[4];

  v4 = v3 << 11;
  if ( a1 == 0 )
    return -2147352169;
  v7 = sceKernelCpuSuspendIntr(a1, a2);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x50C), v15);
  tman_cfc0(13);
  v8 = v15[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352168;
  }
  v9 = *(u16 *)(v15[0] + 22) & 2;
  if ( v4 >= 0 )
    v9 = 2;
  if ( v9 == 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352367;
  }
  v10 = (int *)(v15[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21));
  if ( v2 == 0 )
  {
    v8 = TMAN_BSS_U32(0x40);
    if ( v10 == (int *)TMAN_BSS_U32(0x40) )
    {
      sceKernelCpuResumeIntr(v7);
      return -2147352169;
    }
  }
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(54, v10, 1, a1);
  switch ( v10[3] )
  {
    case 1:
      if ( TMAN_BSS_U32(0x4C) != 2 )
        goto LABEL_17;
      v13 = 0;
      TMAN_BSS_U32(0x4C) = 1;
      break;
    case 8:
      v11 = *v10;
      v12 = v10[1];
      v10[3] = 2;
      *(u32 *)(v11 + 4) = v12;
      *(u32 *)v10[1] = v11;
      if ( v2 != 0 )
      {
        tman_thread_fn_5(v10, v8, v11, v12);
        TMAN_BSS_U32(0x44) = 0;
      }
      else
      {
        tman_thread_fn_16(v10, v7);
      }
      v13 = 0;
      break;
    case 12:
      v13 = 0;
      v10[3] = 4;
      break;
    default:
LABEL_17:
      v13 = -2147352155;
      break;
  }
  sceKernelCpuResumeIntr(v7);
  return v13;
}


int sceKernelSuspendAllUserThreads(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  u32 *v5;
  int v6;
  int i;
  u32 *v8;
  int v9;
  int v10;
  int v12;
  u32 *v13;

  tman_cfc0(13);
  if ( v2 != 0 )
    return -2147352476;
  v3 = sceKernelCpuSuspendIntr(a1, a2);
  v4 = TMAN_BSS_U32(0x50C);
  v5 = *(u32 **)TMAN_BSS_U32(0x50C);
  v6 = v3;
  if ( *(u32 *)v4 != v4 )
  {
    for ( i = *(u8 *)(TMAN_BSS_U32(0x50C) + 21); ; i = *(u8 *)(v4 + 21) )
    {
      v8 = &v5[i];
      v9 = v8[5];
      if ( v9 < 0 )
      {
        v10 = v8[3];
      }
      else
      {
        if ( (v9 & 0x8000000) == 0 )
        {
LABEL_7:
          v5 = (u32 *)*v5;
          goto LABEL_8;
        }
        v10 = v8[3];
      }
      switch ( v10 )
      {
        case 0:
        case 1:
        case 3:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 32:
          goto LABEL_7;
        case 2:
          tman_thread_fn_15(&v5[i], v8[4]);
          v12 = TMAN_BSS_U32(0x4F4);
          *v8 = &TMAN_BSS_U32(0x4F0);
          v8[1] = v12;
          TMAN_BSS_U32(0x4F4) = (int)v8;
          v13 = (u32 *)v8[1];
          v8[3] = 8;
          *v13 = v8;
          goto LABEL_7;
        case 4:
          v8[3] = 12;
          goto LABEL_7;
        default:
          v5 = (u32 *)*v5;
          break;
      }
LABEL_8:
      if ( v5 == (u32 *)v4 )
        break;
    }
  }
  TMAN_BSS_U32(0x70) = 1;
  sceKernelCpuResumeIntr(v6);
  return 0;
}


int sceKernelWaitThreadEnd(int a1, int a2, int a3, int a4)
{
  int v4;
  int v5;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v14;
  int v15;
  int v16;
  int v17;
  u8 v18[16];
  int v19;
  int v20;
  int v21;

  v21 = v5;
  v20 = a1;
  v7 = a2 + 4;
  v8 = (v7 | a2) & (v4 << 11);
  v9 = v4 << 11;
  if ( v8 < 0 )
    return -2147352365;
  if ( v20 == 0 )
    return -2147352169;
  tman_cfc0(13);
  if ( a4 != 0 )
    return -2147352476;
  v10 = sceKernelCpuSuspendIntr(v7 | a2, v7);
  v12 = v10;
  if ( v10 == 0 || TMAN_BSS_U32(0x4C) != 0 )
  {
    sceKernelCpuResumeIntr(v10);
    return -2147352153;
  }
  tman_thread_fn_17(v11);
  if ( sceKernelGetUIDcontrolBlockWithType(v20, TMAN_BSS_U32(0x50C), &v19) != 0 )
  {
    sceKernelCpuResumeIntr(v12);
    return -2147352168;
  }
  v14 = *(u16 *)(v19 + 22) & 4;
  if ( v9 >= 0 )
    v14 = 4;
  if ( v14 == 0 )
  {
    sceKernelCpuResumeIntr(v12);
    return -2147352367;
  }
  v15 = v19 + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
  if ( v15 == TMAN_BSS_U32(0x40) )
  {
    sceKernelCpuResumeIntr(v12);
    return -2147352169;
  }
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(55, &TMAN_BSS_U32(0x40), 1, v20);
  v16 = TMAN_BSS_U32(0x40);
  if ( a2 != 0 )
    tman_mem_fn_34(v18, *(u32 *)a2);
  if ( *(u32 *)(v15 + 12) == 0x10 || *(u32 *)(v15 + 12) == 0x20 )
  {
    v17 = *(u32 *)(v15 + 84);
  }
  else
  {
    *(u32 *)(v16 + 152) = 0;
    v17 = tman_sched_fn_5(v19, 9, 0, (int)v18, a2);
  }
  sceKernelCpuResumeIntr(v12);
  return v17;
}


void sceKernelDelayThread()
{
  JUMPOUT(0x3E10);
}


int sceKernelDelayThreadCB(int a1, int a2)
{
  int v2;
  int v3;
  int v5;
  int v6;
  u32 *v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  u8 v18[16];
  int v19;
  int v20;

  v20 = v3;
  tman_cfc0(13);
  if ( v2 != 0 )
    return -2147352476;
  v5 = sceKernelCpuSuspendIntr(a1, a2);
  v6 = v5;
  if ( v5 != 0 && TMAN_BSS_U32(0x4C) == 0 )
  {
    tman_thread_fn_17(0);
    v8 = (u32 *)TMAN_BSS_U32(0x40);
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((int (*)())TMAN_BSS_U32(0x48))(135, &TMAN_BSS_U32(0x40), 1, a1);
    tman_mem_fn_34(v18, a1);
    v11 = -2147352576;
    v12 = -2147352152;
    v19 = -2147352152;
    v13 = 0x20000u;
    v14 = -2147352576;
    while ( 1 )
    {
      v15 = v8[39];
      if ( v15 != 0 )
        tman_sched_fn_21(v6, v9, v14, v12, v11, v13, v15, v10);
      v8[31] = 0;
      v16 = tman_mem_fn_36(v18, tman_sched_fn_34, v8);
      v17 = v16;
      if ( v16 < 0 )
        break;
      if ( v8[39] != 0 )
      {
        tman_mem_fn_37(v16, 0);
        v8[30] = 0;
        v8[32] = -2147352149;
      }
      else
      {
        v8[30] = v16;
        v8[24] = a1;
        v8[25] = 0;
        tman_sched_fn_3((int)&TMAN_BSS_U32(0x4E0), 2);
        v8[38] = 1;
        TMAN_BSS_U32(0x44) = 0;
        tman_sched_fn_19(1, 1);
      }
      if ( v8[32] != -2147352149 )
      {
        sceKernelCpuResumeIntr(v6);
        return v8[32];
      }
    }
    if ( v16 == v19 )
      v17 = 0;
    sceKernelCpuResumeIntr(v6);
    return v17;
  }
  else
  {
    sceKernelCpuResumeIntr(v5);
    return -2147352153;
  }
}


void sceKernelDelaySysClockThread()
{
  JUMPOUT(0x4034);
}


int sceKernelDelaySysClockThreadCB(unsigned int a1)
{
  int v1;
  int v2;
  int v3;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 *v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  u8 v21[16];
  int v22;
  int v23;

  v23 = v3;
  tman_cfc0(13);
  v5 = v2 << 11;
  if ( v1 != 0 )
    return -2147352476;
  v6 = (a1 + 8) | a1;
  if ( (v6 & v5) < 0 )
    return -2147352365;
  v7 = sceKernelCpuSuspendIntr(v6 & v5, v6);
  v9 = v7;
  if ( v7 != 0 && TMAN_BSS_U32(0x4C) == 0 )
  {
    tman_thread_fn_17(v8);
    v11 = (u32 *)TMAN_BSS_U32(0x40);
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((void (*)(int, int *, int, u32, u32))TMAN_BSS_U32(0x48))(
        137,
        &TMAN_BSS_U32(0x40),
        2,
        *(u32 *)(a1 + 4),
        *(u32 *)a1);
    tman_mem_fn_35(v21, a1);
    v17 = -2147352152;
    v22 = -2147352152;
    v18 = -2147352576;
    while ( 1 )
    {
      if ( v11[39] != 0 )
        tman_sched_fn_21(v9, v12, v13, v14, v15, v16, v18, v17);
      v11[31] = 0;
      v19 = tman_mem_fn_36(v21, tman_sched_fn_34, v11);
      v20 = v19;
      if ( v19 < 0 )
        break;
      if ( v11[39] != 0 )
      {
        tman_mem_fn_37(v19, 0);
        v11[30] = 0;
        v11[32] = -2147352149;
      }
      else
      {
        v11[30] = v19;
        v11[24] = *(u32 *)a1;
        v11[25] = *(u32 *)(a1 + 4);
        tman_sched_fn_3((int)&TMAN_BSS_U32(0x4E0), 2);
        v11[38] = 1;
        TMAN_BSS_U32(0x44) = 0;
        tman_sched_fn_19(1, 1);
      }
      if ( v11[32] != -2147352149 )
      {
        sceKernelCpuResumeIntr(v9);
        return v11[32];
      }
    }
    if ( v19 == v22 )
      v20 = 0;
    sceKernelCpuResumeIntr(v9);
    return v20;
  }
  else
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352153;
  }
}


int tman_sched_fn_34(int *a1, int a2)
{
  int v3;
  int v4;

  v3 = sceKernelCpuSuspendIntr(a1, a2);
  a1[30] = 0;
  v4 = v3;
  if ( tman_sched_fn_6(a1) > 0 )
    TMAN_BSS_U32(0x44) = 0;
  return sceKernelCpuResumeIntr(v4);
}


