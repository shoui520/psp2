#include "threadman_internal.h"

int sceKernelCreateSema(int a1, int a2, int a3, int a4, int a5)
{
  int v5;
  int v6;
  int v8;
  int v11;
  int v14;
  int v16;
  int UID;
  int v18;
  int v19;
  void (*v20)(int, u32, int, int);
  u32 *v21;
  int v22;
  u32 v23[4];

  v6 = v5 << 11;
  v8 = (v5 << 11) & a1;
  v11 = v5 << 11;
  if ( v8 < 0 )
    return -2147352365;
  v14 = (a5 + 4) | a5;
  if ( (v14 & v6) < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a4 != 0 )
    return -2147352476;
  if ( (a2 & 0xFFFFFE00) != 0 )
    return -2147352175;
  v16 = sceKernelCpuSuspendIntr(v14 & v6, v14);
  UID = sceKernelCreateUID(TMAN_BSS_U32(0x510), a1, (u8)(a2 | (v11 >> 31)), v23);
  if ( UID != 0 )
  {
    sceKernelCpuResumeIntr(v16);
    return UID;
  }
  else
  {
    v18 = *(u32 *)(v23[0] + 12);
    v19 = TMAN_BSS_U32(0x844);
    v20 = (void (*)(int, u32, int, int))TMAN_BSS_U32(0x48);
    v21 = (u32 *)(v23[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x510) + 21));
    *(u32 *)(v23[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21)) = a2;
    v22 = v18 ^ v19;
    v21[2] = a4;
    *v21 = a5;
    v21[1] = a3;
    v21[3] = a3;
    if ( v20 != NULL )
      v20(57, 0, 1, v18 ^ v19);
    sceKernelCpuResumeIntr(v16);
    return v22;
  }
}


int sceKernelDeleteSema(int a1, int a2)
{
  int v2;
  int v3;
  int v5;
  int v6;
  int v7;
  u32 v9[4];

  v3 = -2147352476;
  tman_cfc0(13);
  v5 = v2 << 11;
  if ( a1 == 0 )
  {
    v6 = sceKernelCpuSuspendIntr(0, a2);
    if ( sceKernelGetUIDcontrolBlockWithType(0, TMAN_BSS_U32(0x510), v9) != 0 )
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352167;
    }
    else
    {
      v7 = *(u16 *)(v9[0] + 22) & 0x10;
      if ( v5 >= 0 )
        v7 = 16;
      if ( v7 != 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((int (*)())TMAN_BSS_U32(0x48))(58, a1, 1, a1);
        sceKernelDeleteUID(0);
        sceKernelCpuResumeIntr(v6);
        return 0;
      }
      else
      {
        sceKernelCpuResumeIntr(v6);
        return -2147352367;
      }
    }
  }
  return v3;
}


int sceKernelSignalSema(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int *v5;
  s64 UIDcontrolBlockWithType;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int *v14;
  int *v15;
  int v16;
  int *i;
  int v18;
  int *v19;
  int v20;
  int j;
  int *v22;
  int *v23;
  int v24;
  int v25;
  int v27;
  int v28;
  int v29;

  v4 = v2 << 11;
  v5 = NULL;
  v9 = sceKernelCpuSuspendIntr(a1, a2);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x510), &v28);
  tman_cfc0(13);
  v29 = HIDWORD(UIDcontrolBlockWithType);
  if ( (u32)UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352167;
  }
  else
  {
    v10 = *(u16 *)(v28 + 22) & 2;
    if ( v4 >= 0 )
      v10 = 2;
    if ( v10 != 0 )
    {
      v11 = v28 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
      v12 = v28 + 4 * *(u8 *)(TMAN_BSS_U32(0x510) + 21);
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((void (*)(int, int, int, int, int))TMAN_BSS_U32(0x48))(59, v3, 2, a1, a2);
      v13 = *(u32 *)(v12 + 4);
      v14 = *(int **)(v11 + 12);
      v15 = (int *)(v11 + 12);
      v16 = v13 + a2;
      for ( i = v14; v14 != v15; v16 -= v18 )
      {
        v18 = v14[24];
        v14 = (int *)*v14;
      }
      if ( *(u32 *)(v12 + 8) < v16 )
      {
        sceKernelCpuResumeIntr(v9);
        return -2147352146;
      }
      else
      {
        v19 = *(int **)(v11 + 12);
        *(u32 *)(v12 + 4) = v13 + a2;
        v20 = 0;
        if ( i != v15 )
        {
          for ( j = TMAN_BSS_U32(0x70); ; j = TMAN_BSS_U32(0x70) )
          {
            v22 = v19;
            v23 = v19;
            v19 = (int *)*v19;
            if ( j != 0 )
            {
              v27 = v22[5];
              if ( v27 < 0 || (v27 & 0x8000000) != 0 )
                goto LABEL_17;
              v24 = *(u32 *)(v12 + 4);
            }
            else
            {
              v24 = *(u32 *)(v12 + 4);
            }
            v25 = v22[24];
            if ( v24 < v25 )
              break;
            *(u32 *)(v12 + 4) = v24 - v25;
            if ( tman_sched_fn_6(v23) > 0 )
            {
              v5 = v22;
              ++v20;
            }
LABEL_17:
            if ( v19 == v15 )
              break;
          }
        }
        if ( v29 != 0 )
        {
          if ( v20 > 0 )
            TMAN_BSS_U32(0x44) = 0;
        }
        else if ( v20 == 1 )
        {
          tman_thread_fn_15(v5, v5[4]);
          tman_thread_fn_16(v5, v9);
        }
        else if ( v20 > 0 )
        {
          TMAN_BSS_U32(0x44) = 0;
          tman_sched_fn_19(0, v9);
        }
        sceKernelCpuResumeIntr(v9);
        return 0;
      }
    }
    else
    {
      sceKernelCpuResumeIntr(v9);
      return -2147352367;
    }
  }
}


int sceKernelWaitSema(int a1, int a2, int a3)
{
  int v3;
  int v4;
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
  u32 *v18;
  int v19;
  int v20;
  u8 v21[16];
  int v22;
  int v23;
  int v24;
  u32 *v25;
  int v26;

  v26 = v4;
  v23 = a1;
  v7 = (a3 + 4) | a3;
  v8 = v7 & (v3 << 11);
  v9 = v3 << 11;
  if ( v8 < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a3 != 0 )
    return -2147352476;
  if ( a2 <= 0 )
    return -2147352131;
  v10 = sceKernelCpuSuspendIntr(v7, a3 + 4);
  v12 = v10;
  if ( v10 == 0 || TMAN_BSS_U32(0x4C) != 0 )
  {
    sceKernelCpuResumeIntr(v10);
    return -2147352153;
  }
  tman_thread_fn_17(v11);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v23, TMAN_BSS_U32(0x510), &v22) != 0 )
  {
    sceKernelCpuResumeIntr(v12);
    return -2147352167;
  }
  v14 = *(u16 *)(v22 + 22) & 4;
  if ( v9 >= 0 )
    v14 = 4;
  v15 = TMAN_BSS_U32(0x40);
  if ( v14 == 0 )
  {
    sceKernelCpuResumeIntr(v12);
    return -2147352367;
  }
  v16 = 4 * *(u8 *)(TMAN_BSS_U32(0x510) + 21);
  v17 = *(u32 *)(v22 + v16 + 8);
  v18 = (u32 *)(v22 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
  v24 = v22 + v16;
  v25 = v18;
  if ( v17 < a2 )
  {
    sceKernelCpuResumeIntr(v12);
    return -2147352131;
  }
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((void (*)(int, int *, int, int, int))TMAN_BSS_U32(0x48))(60, &TMAN_BSS_U32(0x40), 2, v23, a2);
  if ( a3 != 0 )
    tman_mem_fn_34(v21, *(u32 *)a3);
  if ( tman_sync_fn_1(v25) != 0 || (v19 = *(u32 *)(v24 + 4)) < a2 )
  {
    *(u32 *)(v15 + 96) = a2;
    *(u32 *)(v15 + 152) = 0;
    v20 = tman_sched_fn_5(v22, 3, *v25, (int)v21, a3);
  }
  else
  {
    v20 = 0;
    *(u32 *)(v24 + 4) = v19 - a2;
  }
  sceKernelCpuResumeIntr(v12);
  return v20;
}


int sceKernelPollSema(int a1, int a2)
{
  int v2;
  int v4;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v13[4];

  v4 = v2 << 11;
  if ( a2 <= 0 )
    return -2147352131;
  v6 = sceKernelCpuSuspendIntr(a1, a2);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x510), v13) != 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352167;
  }
  v7 = *(u16 *)(v13[0] + 22) & 4;
  if ( v4 >= 0 )
    v7 = 4;
  if ( v7 == 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352367;
  }
  v8 = *(u8 *)(TMAN_BSS_U32(0x510) + 21);
  v9 = v13[0] + 4 * v8;
  v10 = v13[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  if ( *(u32 *)(v9 + 8) < a2 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352131;
  }
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((void (*)(int, int, int, int, int))TMAN_BSS_U32(0x48))(62, v13[0] + 4 * v8, 2, a1, a2);
  if ( tman_sync_fn_1(v10) != 0 || (v11 = *(u32 *)(v9 + 4)) < a2 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352147;
  }
  else
  {
    *(u32 *)(v9 + 4) = v11 - a2;
    sceKernelCpuResumeIntr(v6);
    return 0;
  }
}


int sceKernelCancelSema(int a1, int a2, int *a3)
{
  int v3;
  int v5;
  int v7;
  int UIDcontrolBlockWithType;
  int v10;
  int v11;
  int v12;
  u32 *v13;
  int v14;
  u32 v16[4];

  v5 = ((unsigned int)(a3 + 1) | (unsigned int)a3) & (v3 << 11);
  v7 = v3 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v10 = sceKernelCpuSuspendIntr(a1, a2);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x510), v16);
  v11 = v16[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v10);
    return -2147352167;
  }
  else
  {
    v12 = *(u16 *)(v16[0] + 22) & 8;
    if ( v7 >= 0 )
      v12 = 8;
    if ( v12 != 0 )
    {
      v13 = (u32 *)(v16[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x510) + 21));
      if ( v13[2] < a2 )
      {
        sceKernelCpuResumeIntr(v10);
        return -2147352131;
      }
      else
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
        {
          ((void (*)(int, u32 *, int, int, int, int *))TMAN_BSS_U32(0x48))(63, v13, 3, a1, a2, a3);
          v11 = v16[0];
        }
        v14 = sceKernelCallUIDObjFunction(v11, -2032580477, v11);
        if ( a2 < 0 )
          v13[1] = v13[3];
        else
          v13[1] = a2;
        if ( a3 != NULL )
          *a3 = v14;
        sceKernelCpuResumeIntr(v10);
        return 0;
      }
    }
    else
    {
      sceKernelCpuResumeIntr(v10);
      return -2147352367;
    }
  }
}


int tman_sync_fn_1(int *a1)
{
  int v1;
  int *v2;
  int v3;
  int v4;

  v1 = 0;
  if ( a1[2] > 0 )
  {
    v2 = (int *)a1[3];
    if ( v2 != a1 + 3 )
    {
      while ( 1 )
      {
        if ( TMAN_BSS_U32(0x70) == 0 )
        {
          v4 = *a1;
          goto LABEL_6;
        }
        v3 = v2[5];
        if ( v3 >= 0 && (v3 & 0x8000000) == 0 )
          break;
        v2 = (int *)*v2;
        if ( v2 == a1 + 3 )
          return v1;
      }
      v4 = *a1;
LABEL_6:
      if ( (v4 & 0x100) != 0 )
      {
        if ( *(u32 *)(TMAN_BSS_U32(0x40) + 16) >= v2[4] )
          return 1;
      }
      else
      {
        return 1;
      }
    }
  }
  return v1;
}


int sceKernelReferSemaStatus(int a1, unsigned int *a2)
{
  int v2;
  int v3;
  int v5;
  int v6;
  int v7;
  int UIDcontrolBlockWithType;
  int v10;
  int v11;
  int v12;
  int *v13;
  u32 *v14;
  int *v15;
  int v16;
  unsigned int v17;
  u32 v19[4];

  v5 = (unsigned int)(a2 + 14) | (unsigned int)a2;
  v6 = v5 & (v2 << 11);
  v7 = v2 << 11;
  if ( v6 < 0 )
    return -2147352365;
  v10 = sceKernelCpuSuspendIntr(a1, v5);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x510), v19);
  v11 = v19[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v10);
    return -2147352167;
  }
  else
  {
    v12 = *(u16 *)(v19[0] + 22) & 1;
    if ( v7 >= 0 )
      v12 = 1;
    if ( v12 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int, int, int, unsigned int *))TMAN_BSS_U32(0x48))(64, v3, 2, a1, a2);
        v11 = v19[0];
      }
      v13 = (int *)&TMAN_BSS_U8(0x6DC);
      v14 = (u32 *)(v11 + 4 * *(u8 *)(TMAN_BSS_U32(0x510) + 21));
      v15 = (int *)(v11 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
      do
      {
        ++v13;
        v13[13] = 0;
      }
      while ( v13 != &TMAN_BSS_U32(0x714) );
      v16 = *(u32 *)(v11 + 16);
      TMAN_BSS_U32(0x714) = 56;
      if ( v16 != 0 )
        strncpy(&TMAN_BSS_U32(0x718), v16, 31);
      TMAN_BSS_U32(0x738) = *v15;
      v17 = *a2;
      TMAN_BSS_U32(0x748) = v15[2];
      if ( TMAN_BSS_U32(0x714) < v17 )
        v17 = TMAN_BSS_U32(0x714);
      TMAN_BSS_U32(0x740) = v14[1];
      TMAN_BSS_U32(0x744) = v14[2];
      TMAN_BSS_U32(0x73C) = v14[3];
      memcpy(a2, &TMAN_BSS_U32(0x714), v17);
      sceKernelCpuResumeIntr(v10);
      return 0;
    }
    else
    {
      sceKernelCpuResumeIntr(v10);
      return -2147352367;
    }
  }
}


int tman_sync_fn_2()
{
  sceKernelCreateUIDtypeInherit("WaitQ", "Semaphore", 48, g_uid_sema_funcs, 0, &TMAN_BSS_U32(0x510));
  return 0;
}


int tman_sync_fn_3(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_sync_fn_4(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return 0;
}


int tman_sync_fn_5(int a1)
{
  int v1;
  int v2;
  int v3;
  int result;
  int *v5;
  int *v6;
  int i;
  int *v8;
  int *v9;
  int v10;
  int v11;
  int v12;

  v1 = 0;
  v2 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  v3 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x510) + 21);
  result = 0;
  if ( *(int *)(v2 + 8) > 0 )
  {
    v5 = *(int **)(v2 + 12);
    v6 = (int *)(v2 + 12);
    if ( v5 != (int *)(v2 + 12) )
    {
      for ( i = TMAN_BSS_U32(0x70); ; i = TMAN_BSS_U32(0x70) )
      {
        v8 = v5;
        v9 = v5;
        v5 = (int *)*v5;
        if ( i != 0 )
        {
          v12 = v8[5];
          if ( v12 < 0 || (v12 & 0x8000000) != 0 )
            goto LABEL_8;
          v10 = *(u32 *)(v3 + 4);
        }
        else
        {
          v10 = *(u32 *)(v3 + 4);
        }
        v11 = v8[24];
        if ( v10 < v11 )
          return v1;
        *(u32 *)(v3 + 4) = v10 - v11;
        v1 += tman_sched_fn_6(v9) > 0;
LABEL_8:
        if ( v5 == v6 )
          return v1;
      }
    }
    return v1;
  }
  return result;
}


int sceKernelCreateEventFlag(int a1, int a2, int a3, int a4)
{
  int v4;
  int v5;
  int v7;
  int v9;
  int v12;
  int v14;
  int UID;
  int v16;
  int v17;
  void (*v18)(int, u32, int, int);
  u32 *v19;
  int v20;
  u32 v21[4];

  v5 = v4 << 11;
  v7 = (v4 << 11) & a1;
  v9 = v4 << 11;
  if ( v7 < 0 )
    return -2147352365;
  v12 = (a4 + 4) | a4;
  if ( (v12 & v5) < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a4 != 0 )
    return -2147352476;
  if ( (a2 & 0xFFFFFD00) != 0 )
    return -2147352175;
  v14 = sceKernelCpuSuspendIntr(v12 & v5, v12);
  UID = sceKernelCreateUID(TMAN_BSS_U32(0x514), a1, (u8)(a2 | (v9 >> 31)), v21);
  if ( UID != 0 )
  {
    sceKernelCpuResumeIntr(v14);
    return UID;
  }
  else
  {
    v16 = *(u32 *)(v21[0] + 12);
    v17 = TMAN_BSS_U32(0x844);
    v18 = (void (*)(int, u32, int, int))TMAN_BSS_U32(0x48);
    v19 = (u32 *)(v21[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x514) + 21));
    *(u32 *)(v21[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21)) = a2;
    v20 = v16 ^ v17;
    *v19 = a4;
    v19[2] = a3;
    v19[3] = 0;
    v19[1] = a3;
    v19[4] = 0;
    if ( v18 != NULL )
      v18(65, 0, 1, v16 ^ v17);
    sceKernelCpuResumeIntr(v14);
    return v20;
  }
}


int sceKernelDeleteEventFlag(int a1, int a2)
{
  int v2;
  int v3;
  int v5;
  int v6;
  int v7;
  u32 v9[4];

  v3 = -2147352476;
  tman_cfc0(13);
  v5 = v2 << 11;
  if ( a1 == 0 )
  {
    v6 = sceKernelCpuSuspendIntr(0, a2);
    if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(0, TMAN_BSS_U32(0x514), v9) != 0 )
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352166;
    }
    else
    {
      v7 = *(u16 *)(v9[0] + 22) & 0x10;
      if ( v5 >= 0 )
        v7 = 16;
      if ( v7 != 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((int (*)())TMAN_BSS_U32(0x48))(66, a1, 1, a1);
        sceKernelDeleteUID(0);
        sceKernelCpuResumeIntr(v6);
        return 0;
      }
      else
      {
        sceKernelCpuResumeIntr(v6);
        return -2147352367;
      }
    }
  }
  return v3;
}


int sceKernelSetEventFlag(int a1, int a2)
{
  int v2;
  int v3;
  s64 UIDcontrolBlockWithType;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int *v13;
  int *v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int *v19;
  int v20;
  int v21;
  int v23;
  int v24;
  int *v25;
  int v26;

  v3 = v2 << 11;
  v7 = sceKernelCpuSuspendIntr(a1, a2);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x514), &v24);
  tman_cfc0(13);
  v25 = NULL;
  v26 = HIDWORD(UIDcontrolBlockWithType);
  if ( (u32)UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352166;
  }
  else
  {
    v8 = *(u16 *)(v24 + 22) & 2;
    if ( v3 >= 0 )
      v8 = 2;
    if ( v8 != 0 )
    {
      v9 = *(u8 *)(TMAN_BSS_U32(0x508) + 21);
      v10 = v24 + 4 * v9;
      v11 = v24 + 4 * *(u8 *)(TMAN_BSS_U32(0x514) + 21);
      if ( a2 != 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((void (*)(int, int, int, int, int))TMAN_BSS_U32(0x48))(67, v24 + 4 * v9, 2, a1, a2);
        v12 = *(u32 *)(v11 + 4);
        v13 = *(int **)(v10 + 12);
        v14 = (int *)(v10 + 12);
        *(u32 *)(v11 + 4) = v12 | a2;
        v15 = 0;
        if ( v13 == (int *)(v10 + 12) )
        {
LABEL_18:
          v16 = v26;
        }
        else if ( (v12 | a2) != 0 )
        {
          v17 = v12 | a2;
          while ( 1 )
          {
            v18 = v13[24];
            v19 = v13;
            v20 = v13[25] & 1;
            v13 = (int *)*v13;
            v21 = v17 & v18;
            if ( v20 == 0 )
              v21 = v21 == v18;
            if ( v21 != 0 )
            {
              if ( tman_sched_fn_6(v19) > 0 )
              {
                v25 = v19;
                ++v15;
              }
              v23 = v19[25];
              if ( (v23 & 0x10) != 0 )
              {
                *(u32 *)(v11 + 4) = 0;
                v23 = v19[25];
              }
              if ( (v23 & 0x20) != 0 )
                *(u32 *)(v11 + 4) &= ~v18;
            }
            if ( v13 == v14 )
              break;
            v17 = *(u32 *)(v11 + 4);
            if ( v17 == 0 )
              goto LABEL_18;
          }
          v16 = v26;
        }
        else
        {
          v16 = v26;
        }
        if ( v16 != 0 )
        {
          if ( v15 > 0 )
            TMAN_BSS_U32(0x44) = 0;
        }
        else if ( v15 == 1 )
        {
          tman_thread_fn_15(v25, v25[4]);
          tman_thread_fn_16(v25, v7);
        }
        else if ( v15 > 0 )
        {
          TMAN_BSS_U32(0x44) = 0;
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


int sceKernelClearEventFlag(int a1, int a2)
{
  int v2;
  int v3;
  int v6;
  int v7;
  int v8;
  u32 v10[4];

  v6 = sceKernelCpuSuspendIntr(a1, a2);
  v3 = v2 << 11;
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x514), v10) != 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352166;
  }
  else
  {
    v7 = *(u16 *)(v10[0] + 22) & 2;
    if ( v3 >= 0 )
      v7 = 2;
    if ( v7 != 0 )
    {
      v8 = v10[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x514) + 21);
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((int (*)())TMAN_BSS_U32(0x48))(68, &TMAN_BSS_U32(0x40), 2, a1);
      *(u32 *)(v8 + 4) &= a2;
      sceKernelCpuResumeIntr(v6);
      return 0;
    }
    else
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352367;
    }
  }
}


void sceKernelWaitEventFlag()
{
  JUMPOUT(0x5814);
}


int sceKernelWaitEventFlagCB(int a1, int a2, int a3, unsigned int a4, int a5, int a6)
{
  int v6;
  int v7;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v18;
  int v19;
  int v20;
  u32 *v21;
  u32 *v22;
  int v23;
  u32 *v24;
  int v25;
  u32 *v26;
  int *v27;
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
  u32 *v40;
  int v41;
  u8 v42[16];
  int v43;
  int v44;
  int v45;
  int v46;
  int v47;
  int v48;
  int v49;
  u32 *v50;
  int v51;
  int v52;
  int v53;

  v53 = v7;
  v44 = a1;
  v12 = (a4 + 4) | a4;
  v13 = v6 << 11;
  v14 = v12 & (v6 << 11);
  v15 = v6 << 11;
  if ( v14 < 0 )
    return -2147352365;
  v16 = ((a5 + 4) | a5) & v13;
  if ( v16 < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a6 != 0 )
    return -2147352476;
  if ( (a3 & 0xFFFFFFCE) != 0 || (a3 & 0x30) == 0x30 )
    return -2147352171;
  if ( a2 == 0 )
    return -2147352143;
  v45 = sceKernelCpuSuspendIntr(v16, v12);
  if ( v45 == 0 || TMAN_BSS_U32(0x4C) != 0 )
  {
    sceKernelCpuResumeIntr(v45);
    return -2147352153;
  }
  tman_thread_fn_17(v18);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v44, TMAN_BSS_U32(0x514), &v43) != 0 )
  {
    sceKernelCpuResumeIntr(v45);
    return -2147352166;
  }
  else
  {
    v19 = (unsigned int)v15 >> 31;
    v20 = *(u16 *)(v43 + 22) & 4;
    if ( v15 >= 0 )
      v20 = 4;
    if ( v20 == 0 )
    {
      sceKernelCpuResumeIntr(v45);
      return -2147352367;
    }
    v21 = (u32 *)(v43 + 4 * *(u8 *)(TMAN_BSS_U32(0x514) + 21));
    v22 = (u32 *)TMAN_BSS_U32(0x40);
    if ( (*(u32 *)(v43 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21)) & 0x200) == 0 )
    {
      v23 = v21[3];
      v24 = v21 + 5;
      if ( v23 != 0 )
      {
        if ( v23 != TMAN_BSS_U32(0x40) )
        {
          sceKernelCpuResumeIntr(v45);
          return -2147352144;
        }
        v19 = v21[4];
      }
      else
      {
        v25 = *(u32 *)(TMAN_BSS_U32(0x40) + 180);
        v21[5] = TMAN_BSS_U32(0x40) + 176;
        v21[6] = v25;
        v22[45] = v24;
        v26 = (u32 *)v21[6];
        v21[3] = v22;
        *v26 = v24;
        v19 = v21[4];
      }
      v21[4] = v19 + 1;
    }
    v27 = &TMAN_BSS_U32(0x40);
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((void (*)(int, unsigned int, int, int, int, int, unsigned int))TMAN_BSS_U32(0x48))(
        70,
        0x20000u,
        4,
        v44,
        a2,
        a3,
        a4);
    v28 = -2147352576;
    if ( a5 != 0 )
    {
      tman_mem_fn_34(v42, *(u32 *)a5);
      v28 = -2147352576;
    }
    v29 = a3 & 0x10;
    v30 = a3 & 0x20;
    v31 = ~a2;
    v32 = (int)(v21 + 5);
    v46 = a3 & 1;
    v47 = v29;
    v48 = v30;
    v49 = ~a2;
    v50 = v21 + 5;
    v51 = 1;
    v52 = -2147352149;
    while ( 1 )
    {
      v33 = v46;
      v34 = v45;
      if ( v22[39] != 0 )
      {
        if ( a4 != 0 )
          *(u32 *)a4 = v21[1];
        tman_sched_fn_21(v34, v19, (int)v27, v28, v32, v31, v30, v29);
        if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v44, TMAN_BSS_U32(0x514), &v43) != 0 )
        {
          sceKernelCpuResumeIntr(v45);
          return -2147352139;
        }
        v33 = v46;
      }
      v35 = v21[1];
      v36 = v33 != 0 ? v35 & a2 : (v35 & a2) == a2;
      if ( v36 != 0 )
        break;
      v22[24] = a2;
      v22[25] = a3;
      v22[26] = a4;
      v22[38] = 1;
      v37 = tman_sched_fn_5(v43, 4, 0, (int)v42, a5);
      v29 = v37 == v52;
      v30 = v51 & v29;
      v38 = v37;
      if ( (v51 & v29) == 0 )
      {
        sceKernelCpuResumeIntr(v45);
        return v38;
      }
    }
    if ( a4 != 0 )
      *(u32 *)a4 = v35;
    if ( v47 != 0 )
      v21[1] = 0;
    if ( v48 != 0 )
      v21[1] &= v49;
    if ( v21[3] != 0 )
    {
      v39 = v21[4];
      v21[4] = v39 - 1;
      if ( v39 == 1 )
      {
        v40 = v50;
        v41 = v21[5];
        v21[3] = 0;
        *(u32 *)(v41 + 4) = v40[1];
        *(u32 *)v40[1] = v41;
      }
    }
    sceKernelCpuResumeIntr(v45);
    return 0;
  }
}


int sceKernelPollEventFlag(int a1, int a2, int a3, unsigned int a4)
{
  int v4;
  int v6;
  int v8;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  u32 v18[4];

  v6 = ((a4 + 4) | a4) & (v4 << 11);
  v8 = v4 << 11;
  if ( v6 < 0 )
    return -2147352365;
  if ( (a3 & 0xFFFFFFCE) != 0 || (a3 & 0x30) == 0x30 )
    return -2147352171;
  if ( a2 == 0 )
    return -2147352143;
  v11 = sceKernelCpuSuspendIntr(0, -50);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x514), v18) != 0 )
  {
    sceKernelCpuResumeIntr(v11);
    return -2147352166;
  }
  else
  {
    v12 = *(u16 *)(v18[0] + 22) & 4;
    if ( v8 >= 0 )
      v12 = 4;
    if ( v12 != 0 )
    {
      v13 = v18[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x514) + 21);
      if ( (*(u32 *)(v18[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21)) & 0x200) != 0
        || (v14 = *(u32 *)(v13 + 12)) == 0
        || v14 == TMAN_BSS_U32(0x40) )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((void (*)(int, int, int, int, int, int, unsigned int))TMAN_BSS_U32(0x48))(71, v13, 4, a1, a2, a3, a4);
        v15 = *(u32 *)(v13 + 4);
        if ( (a3 & 1) != 0 )
          v16 = v15 & a2;
        else
          v16 = (v15 & a2) == a2;
        if ( a4 != 0 )
          *(u32 *)a4 = v15;
        if ( v16 != 0 )
        {
          if ( (a3 & 0x10) != 0 )
            *(u32 *)(v13 + 4) = 0;
          if ( (a3 & 0x20) != 0 )
            *(u32 *)(v13 + 4) &= ~a2;
          sceKernelCpuResumeIntr(v11);
          return 0;
        }
        else
        {
          sceKernelCpuResumeIntr(v11);
          return -2147352145;
        }
      }
      else
      {
        sceKernelCpuResumeIntr(v11);
        return -2147352144;
      }
    }
    else
    {
      sceKernelCpuResumeIntr(v11);
      return -2147352367;
    }
  }
}


int sceKernelCancelEventFlag(int a1, int a2, int *a3)
{
  int v3;
  int v5;
  int v8;
  int UIDcontrolBlockWithType;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  u32 v16[4];

  v5 = ((unsigned int)(a3 + 1) | (unsigned int)a3) & (v3 << 11);
  v8 = v3 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v10 = sceKernelCpuSuspendIntr(a1, a2);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x514), v16);
  v11 = v16[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v10);
    return -2147352166;
  }
  else
  {
    v12 = *(u16 *)(v16[0] + 22) & 8;
    if ( v8 >= 0 )
      v12 = 8;
    if ( v12 != 0 )
    {
      v13 = v16[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x514) + 21);
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int, int, int, int, int *))TMAN_BSS_U32(0x48))(72, v13, 3, a1, a2, a3);
        v11 = v16[0];
      }
      *(u32 *)(v13 + 4) = a2;
      v14 = sceKernelCallUIDObjFunction(v11, -2032580477, v11);
      if ( a3 != NULL )
        *a3 = v14;
      sceKernelCpuResumeIntr(v10);
      return 0;
    }
    else
    {
      sceKernelCpuResumeIntr(v10);
      return -2147352367;
    }
  }
}


int sceKernelReferEventFlagStatus(int a1, unsigned int *a2)
{
  int v2;
  int v4;
  int v5;
  int v7;
  int UIDcontrolBlockWithType;
  int v9;
  int v10;
  int v11;
  int *v12;
  int v13;
  int *v14;
  int v15;
  unsigned int v16;
  u32 v18[4];

  v4 = (unsigned int)(a2 + 13) | (unsigned int)a2;
  v5 = v4 & (v2 << 11);
  v7 = v2 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v9 = sceKernelCpuSuspendIntr(a1, v4);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x514), v18);
  v10 = v18[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352166;
  }
  else
  {
    v11 = *(u16 *)(v18[0] + 22) & 1;
    if ( v7 >= 0 )
      v11 = 1;
    if ( v11 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int, int, int, unsigned int *))TMAN_BSS_U32(0x48))(73, a1, 2, a1, a2);
        v10 = v18[0];
      }
      v12 = (int *)&TMAN_BSS_U8(0x6E0);
      v13 = v10 + 4 * *(u8 *)(TMAN_BSS_U32(0x514) + 21);
      v14 = (int *)(v10 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
      do
      {
        ++v12;
        v12[12] = 0;
      }
      while ( v12 != &TMAN_BSS_U32(0x714) );
      v15 = *(u32 *)(v10 + 16);
      TMAN_BSS_U32(0x714) = 52;
      if ( v15 != 0 )
        strncpy(&TMAN_BSS_U32(0x718), v15, 31);
      TMAN_BSS_U32(0x738) = *v14;
      v16 = *a2;
      TMAN_BSS_U32(0x73C) = *(u32 *)(v13 + 8);
      if ( TMAN_BSS_U32(0x714) < v16 )
        v16 = TMAN_BSS_U32(0x714);
      TMAN_BSS_U32(0x740) = *(u32 *)(v13 + 4);
      TMAN_BSS_U32(0x744) = v14[2];
      memcpy(a2, &TMAN_BSS_U32(0x714), v16);
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


int tman_sync_fn_6(int a1)
{
  u32 *v1;
  u32 *i;
  int v3;
  int result;
  u32 *v5;

  v1 = *(u32 **)(a1 + 176);
  for ( i = v1 - 5; v1 != (u32 *)(a1 + 176); i = v1 - 5 )
  {
    v1 = (u32 *)*v1;
    if ( i[3] != 0 )
    {
      v3 = i[5];
      result = i[6];
      i[3] = 0;
      *(u32 *)(v3 + 4) = result;
      v5 = (u32 *)i[6];
      i[4] = 0;
      *v5 = v3;
    }
  }
  return result;
}


int tman_sync_fn_7()
{
  sceKernelCreateUIDtypeInherit("WaitQ", "EventFlag", 48, g_uid_evf_funcs, 0, &TMAN_BSS_U32(0x514));
  return 0;
}


int tman_sync_fn_8(int a1, int a2, int a3, int *a4)
{
  int v4;
  int v5;
  u32 *v6;
  u32 *v7;
  int v8;
  int v9;
  int v10;

  v4 = *a4;
  v5 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  v6 = (u32 *)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x514) + 21));
  --*(u32 *)(v5 + 8);
  if ( v4 != 0 )
  {
    if ( *(u32 *)(v4 + 128) == -2147352149 )
    {
      v7 = *(u32 **)(v4 + 104);
    }
    else if ( v6[3] != 0 )
    {
      v8 = v6[4];
      v6[4] = v8 - 1;
      if ( v8 == 1 )
      {
        v9 = v6[5];
        v10 = v6[6];
        v6[3] = 0;
        *(u32 *)(v9 + 4) = v10;
        *(u32 *)v6[6] = v9;
      }
      v7 = *(u32 **)(v4 + 104);
    }
    else
    {
      v7 = *(u32 **)(v4 + 104);
    }
    if ( v7 != NULL )
      *v7 = v6[1];
  }
  return 0;
}


int tman_sync_fn_9(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_sync_fn_10(int a1, int a2, int a3, int a4)
{
  u32 *v4;
  int v5;

  v4 = (u32 *)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x514) + 21));
  if ( v4[3] != 0 )
  {
    v5 = v4[5];
    *(u32 *)(v5 + 4) = v4[6];
    *(u32 *)v4[6] = v5;
  }
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return 0;
}


int sceKernelCreateMutex(int a1, int a2, int a3, int *a4)
{
  int v4;
  int v5;
  int v7;
  int v9;
  int v12;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int UID;
  int v21;
  u32 *v22;
  u32 v23[4];

  v5 = v4 << 11;
  v7 = (v4 << 11) & a1;
  v9 = v4 << 11;
  if ( v7 < 0 )
    return -2147352365;
  v12 = (unsigned int)(a4 + 2) | (unsigned int)a4;
  if ( (v12 & v5) < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a4 != NULL )
    return -2147352476;
  if ( (a2 & 0xFFFFF000) != 0 || v5 < 0 && (a2 & 0x400) != 0 )
    return -2147352175;
  if ( a3 < 0 || a3 >= 2 && (a2 & 0x200) == 0 )
    return -2147352131;
  v14 = sceKernelCpuSuspendIntr(v12 & v5, v12);
  TMAN_BSS_U32(0x714) = 0;
  TMAN_BSS_U32(0x718) = 0;
  if ( a4 != NULL )
  {
    v15 = *a4;
    if ( (unsigned int)*a4 >= 9 )
      v15 = 8;
    memcpy(&TMAN_BSS_U32(0x714), a4, v15);
  }
  v16 = TMAN_BSS_U32(0x40);
  v17 = 127;
  if ( (a2 & 0x400) == 0 )
    goto LABEL_24;
  v17 = TMAN_BSS_U32(0x718);
  if ( TMAN_BSS_U32(0x718) == 0 )
    v17 = *(u32 *)(TMAN_BSS_U32(0x40) + 16);
  v18 = (unsigned int)(v17 - 8) < 0x70;
  if ( v9 >= 0 )
    v18 = (unsigned int)(v17 - 1) < 0x7E;
  v19 = v14;
  if ( !v18 || a3 > 0 && (v19 = v14, *(s16 *)(TMAN_BSS_U32(0x40) + 186) < v17) )
  {
    sceKernelCpuResumeIntr(v19);
    return -2147352173;
  }
  else
  {
LABEL_24:
    UID = sceKernelCreateUID(TMAN_BSS_U32(0x518), a1, (u8)(a2 | (v9 >> 31)), v23);
    if ( UID == 0 )
    {
      v21 = v23[0];
      v22 = (u32 *)(v23[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x518) + 21));
      UID = *(u32 *)(v23[0] + 12) ^ TMAN_BSS_U32(0x844);
      *(u32 *)(v23[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21)) = a2;
      v22[3] = a4;
      v22[7] = v17;
      v22[2] = UID;
      v22[6] = 0;
      v22[4] = a3;
      v22[5] = a3;
      if ( a3 > 0 )
        tman_sync_fn_11(v21, v16, a3);
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((int (*)())TMAN_BSS_U32(0x48))(74, UID, 1, UID);
    }
    sceKernelCpuResumeIntr(v14);
    return UID;
  }
}


int sceKernelDeleteMutex(int a1, int a2)
{
  int v2;
  int v3;
  int v5;
  int v6;
  int v7;
  u32 v9[4];

  v3 = -2147352476;
  tman_cfc0(13);
  v5 = v2 << 11;
  if ( a1 == 0 )
  {
    v6 = sceKernelCpuSuspendIntr(0, a2);
    if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(0, TMAN_BSS_U32(0x518), v9) != 0 )
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352125;
    }
    else
    {
      v7 = *(u16 *)(v9[0] + 22) & 0x10;
      if ( v5 >= 0 )
        v7 = 16;
      if ( v7 != 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((int (*)())TMAN_BSS_U32(0x48))(75, a1, 1, a1);
        sceKernelDeleteUID(0);
        sceKernelCpuResumeIntr(v6);
        return 0;
      }
      else
      {
        sceKernelCpuResumeIntr(v6);
        return -2147352367;
      }
    }
  }
  return v3;
}


u32 *tman_sync_fn_11(int a1, u32 *a2, int a3)
{
  int v3;
  u32 *v4;
  u32 *result;
  int v7;
  u32 *v8;
  int v9;
  int i;
  int v11;
  int v12;

  v3 = *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  v4 = (u32 *)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x518) + 21));
  v4[5] = a3;
  result = (u32 *)(a1 + 4 * v3);
  v7 = *result & 0x400;
  v4[6] = a2;
  if ( v7 != 0 )
  {
    v8 = (u32 *)a2[42];
    v9 = v4[7];
    if ( v8 == a2 + 42 )
    {
LABEL_8:
      v11 = v8[1];
    }
    else
    {
      for ( i = v8[7]; v9 >= i; i = v8[7] )
      {
        v8 = (u32 *)*v8;
        if ( v8 == a2 + 42 )
          goto LABEL_8;
      }
      v11 = v8[1];
    }
    v12 = a2[47];
    *v4 = v8;
    v4[1] = v11;
    v8[1] = v4;
    *(u32 *)v4[1] = v4;
    if ( v9 < v12 )
    {
      a2[47] = v9;
      v12 = v9;
    }
    if ( v12 < a2[4] )
      return (u32 *)tman_thread_fn_11(a2, v12, 0);
  }
  else
  {
    result = (u32 *)a2[43];
    *v4 = a2 + 42;
    v4[1] = result;
    a2[43] = v4;
    *(u32 *)v4[1] = v4;
  }
  return result;
}


void tman_sync_fn_12(int a1)
{
  int v1;
  u32 *v2;
  u32 *v3;
  int i;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;

  v1 = 0;
  v2 = (u32 *)(a1 + 168);
  v3 = *(u32 **)(a1 + 168);
  if ( v3 != (u32 *)(a1 + 168) )
  {
    for ( i = TMAN_BSS_U32(0x518); ; i = TMAN_BSS_U32(0x518) )
    {
      v6 = (int)v3;
      v7 = v3[1];
      v8 = (int)&v3[-*(u8 *)(i + 21)];
      v3 = (u32 *)*v3;
      v9 = 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
      v3[1] = v7;
      v10 = *(u32 *)(v8 + v9);
      **(u32 **)(v6 + 4) = v3;
      if ( (v10 & 0x800) != 0 )
      {
        *(u32 *)(v6 + 24) = -1;
      }
      else
      {
        *(u32 *)(v6 + 20) = 0;
        *(u32 *)(v6 + 24) = 0;
        v1 += tman_sched_fn_11(v8, v10, v6);
      }
      if ( v3 == v2 )
        break;
    }
  }
  v11 = *(s16 *)(a1 + 186);
  v12 = *(u32 *)(a1 + 16);
  *(u32 *)(a1 + 188) = 127;
  if ( v12 != v11 )
  {
    *(u32 *)(a1 + 16) = v11;
    v1 = 1;
  }
  if ( v1 != 0 )
  {
    TMAN_BSS_U32(0x44) = 0;
    tman_sched_fn_19(0, 0);
  }
}


int sceKernelUnlockMutex(int a1, int a2)
{
  int v2;
  int v3;
  int *v4;
  int v6;
  int UIDcontrolBlockWithType;
  int v9;
  int v10;
  int v11;
  int v12;
  u32 *v13;
  int v14;
  int v15;
  int v17;
  int v18;
  int v19;
  int v20;
  int *v21;
  int i;
  int v23;
  int v24;
  int v25[4];

  tman_cfc0(13);
  v4 = NULL;
  v6 = v3 << 11;
  if ( v2 != 0 )
    return -2147352476;
  v9 = sceKernelCpuSuspendIntr(a1, a2);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x518), v25);
  v10 = v25[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352125;
  }
  v11 = *(u16 *)(v25[0] + 22) & 2;
  if ( v6 >= 0 )
    v11 = 2;
  if ( v11 == 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352367;
  }
  v12 = *(u8 *)(TMAN_BSS_U32(0x518) + 21);
  v13 = (u32 *)(v25[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
  v14 = v25[0] + 4 * v12;
  if ( a2 > 0 )
  {
    if ( a2 < 2 )
    {
      v15 = *(u32 *)(v14 + 24);
      goto LABEL_12;
    }
    if ( (*v13 & 0x200) != 0 )
    {
      v15 = *(u32 *)(v14 + 24);
LABEL_12:
      if ( v15 == TMAN_BSS_U32(0x40) )
      {
        v17 = a1;
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((void (*)(int, int, int, int, int))TMAN_BSS_U32(0x48))(76, v25[0] + 4 * v12, 2, a1, a2);
        v18 = *(u32 *)(v14 + 20);
        v19 = v18 - a2;
        if ( v18 < a2 )
        {
          sceKernelCpuResumeIntr(v9);
          return -2147352121;
        }
        else
        {
          *(u32 *)(v14 + 20) = v19;
          if ( v19 <= 0 )
          {
            v20 = 0;
            tman_sync_fn_14(v25[0], v11, v10, v17);
            v21 = (int *)v13[3];
            if ( v21 != v13 + 3 )
            {
              for ( i = TMAN_BSS_U32(0x70); ; i = TMAN_BSS_U32(0x70) )
              {
                if ( i == 0 )
                {
                  v24 = v25[0];
                  goto LABEL_23;
                }
                v23 = v21[5];
                if ( v23 >= 0 && (v23 & 0x8000000) == 0 )
                  break;
                v21 = (int *)*v21;
                if ( v21 == v13 + 3 )
                  goto LABEL_25;
              }
              v24 = v25[0];
LABEL_23:
              tman_sync_fn_11(v24, v21, v21[24]);
              if ( tman_sched_fn_6(v21) > 0 )
              {
                v4 = v21;
                v20 = 1;
              }
            }
LABEL_25:
            if ( v20 == 1 )
            {
              tman_thread_fn_15(v4, v4[4]);
              tman_thread_fn_16(v4, v9);
            }
          }
          sceKernelCpuResumeIntr(v9);
          return 0;
        }
      }
      else
      {
        sceKernelCpuResumeIntr(v9);
        return -2147352123;
      }
    }
  }
  sceKernelCpuResumeIntr(v9);
  return -2147352131;
}


void sceKernelLockMutex()
{
  JUMPOUT(0x6B68);
}


int sceKernelLockMutexCB(int a1, int a2, int a3)
{
  int v3;
  int v4;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int UIDcontrolBlockWithType;
  int v15;
  int v16;
  int v17;
  u32 *v18;
  int v19;
  int *v20;
  int *v21;
  int *v22;
  int v23;
  int v24;
  int v25;
  int *v26;
  int v27;
  int v28;
  int v29;
  int v30;
  u8 v31[16];
  int v32;
  int v33;
  int v34;
  int v35;
  int v36;

  v36 = v4;
  v33 = a1;
  v7 = (a3 + 4) | a3;
  v8 = v7 & (v3 << 11);
  v9 = v3 << 11;
  if ( v8 < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a3 != 0 )
    return -2147352476;
  v10 = sceKernelCpuSuspendIntr(v7, 4);
  v12 = v10;
  if ( v10 == 0 || TMAN_BSS_U32(0x4C) != 0 )
  {
    sceKernelCpuResumeIntr(v10);
    return -2147352153;
  }
  tman_thread_fn_17(v11);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(v33, TMAN_BSS_U32(0x518), &v32);
  v16 = v32;
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v12);
    return -2147352125;
  }
  else
  {
    v17 = *(u16 *)(v32 + 22) & 2;
    if ( v9 >= 0 )
      v17 = 2;
    v18 = (u32 *)TMAN_BSS_U32(0x40);
    if ( v17 != 0 )
    {
      v19 = TMAN_BSS_U32(0x508);
      v20 = (int *)(v32 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
      v34 = v32 + 4 * *(u8 *)(TMAN_BSS_U32(0x518) + 21);
      if ( a2 <= 0 || a2 >= 2 && (v15 = *v20, (v16 = *v20 & 0x200) == 0) )
      {
        sceKernelCpuResumeIntr(v12);
        return -2147352131;
      }
      v21 = &TMAN_BSS_U32(0x40);
      v22 = &TMAN_BSS_U32(0x40);
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((void (*)(int, unsigned int, int, int, int))TMAN_BSS_U32(0x48))(78, 0x20000u, 2, v33, a2);
      v23 = v34;
      if ( *(u32 **)(v34 + 24) != v18 )
      {
        if ( a3 != 0 )
          tman_mem_fn_34(v31, *(u32 *)a3);
        v35 = 1;
        while ( 1 )
        {
          if ( v18[39] != 0 )
          {
            tman_sched_fn_21(v12, v19, v16, v15, (int)v22, (int)v21, v23, v17);
            if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v33, TMAN_BSS_U32(0x518), &v32) != 0 )
            {
              sceKernelCpuResumeIntr(v12);
              return -2147352139;
            }
            v24 = v20[2];
          }
          else
          {
            v24 = v20[2];
          }
          v25 = 0;
          if ( v24 <= 0 )
            goto LABEL_33;
          v26 = (int *)v20[3];
          if ( v26 != v20 + 3 )
          {
            while ( 1 )
            {
              if ( TMAN_BSS_U32(0x70) == 0 )
              {
                v28 = *v20;
                goto LABEL_27;
              }
              v27 = v26[5];
              if ( v27 >= 0 && (v27 & 0x8000000) == 0 )
                break;
              v26 = (int *)*v26;
              if ( v26 == v20 + 3 )
                goto LABEL_31;
            }
            v28 = *v20;
LABEL_27:
            if ( (v28 & 0x100) != 0 )
            {
              if ( v18[4] >= v26[4] )
                v25 = 1;
            }
            else
            {
              v25 = 1;
            }
          }
LABEL_31:
          if ( v25 != 0 )
          {
            v18[24] = a2;
          }
          else
          {
LABEL_33:
            if ( *(u32 *)(v34 + 24) == 0 )
            {
              tman_sync_fn_11(v32, v18, a2);
              v29 = 0;
LABEL_36:
              sceKernelCpuResumeIntr(v12);
              return v29;
            }
            v18[24] = a2;
          }
          v18[38] = 1;
          v29 = tman_sched_fn_5(v32, 12, *v20, (int)v31, a3);
          if ( ((u8)v35 & (v29 == 0x800201AB)) == 0 )
            goto LABEL_36;
        }
      }
      if ( (*v20 & 0x200) != 0 )
      {
        v30 = a2 + *(u32 *)(v34 + 20);
        if ( v30 < 0 )
        {
          sceKernelCpuResumeIntr(v12);
          return -2147352122;
        }
        else
        {
          *(u32 *)(v34 + 20) = v30;
          sceKernelCpuResumeIntr(v12);
          return 0;
        }
      }
      else
      {
        sceKernelCpuResumeIntr(v12);
        return -2147352120;
      }
    }
    else
    {
      sceKernelCpuResumeIntr(v12);
      return -2147352367;
    }
  }
}


int sceKernelTryLockMutex(int a1, int a2)
{
  int v2;
  int v3;
  int v6;
  int v7;
  int v8;
  u32 *v9;
  u32 *v10;
  int v11;
  void (*v12)(int, int, int, int, int);
  int v14;
  u32 *v15;
  int v16[4];

  tman_cfc0(13);
  v6 = v3 << 11;
  if ( v2 != 0 )
    return -2147352476;
  v7 = sceKernelCpuSuspendIntr(a1, a2);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x518), v16) != 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352125;
  }
  v8 = *(u16 *)(v16[0] + 22) & 2;
  if ( v6 >= 0 )
    v8 = 2;
  v9 = (u32 *)TMAN_BSS_U32(0x40);
  if ( v8 == 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352367;
  }
  v10 = (u32 *)(v16[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
  v11 = v16[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x518) + 21);
  if ( a2 <= 0 )
    goto LABEL_10;
  if ( a2 >= 2 )
  {
    if ( (*v10 & 0x200) != 0 )
    {
      v12 = (void (*)(int, int, int, int, int))TMAN_BSS_U32(0x48);
      goto LABEL_12;
    }
LABEL_10:
    sceKernelCpuResumeIntr(v7);
    return -2147352131;
  }
  v12 = (void (*)(int, int, int, int, int))TMAN_BSS_U32(0x48);
LABEL_12:
  if ( v12 != NULL )
    v12(79, a2, 2, a1, a2);
  v15 = *(u32 **)(v11 + 24);
  if ( v15 == v9 )
  {
    if ( (*v10 & 0x200) != 0 )
    {
      v14 = a2 + *(u32 *)(v11 + 20);
      if ( v14 >= 0 )
      {
        *(u32 *)(v11 + 20) = v14;
        goto LABEL_17;
      }
      sceKernelCpuResumeIntr(v7);
      return -2147352122;
    }
    else
    {
      sceKernelCpuResumeIntr(v7);
      return -2147352120;
    }
  }
  else
  {
    if ( v15 == NULL )
    {
      tman_sync_fn_11(v16[0], v9, a2);
LABEL_17:
      sceKernelCpuResumeIntr(v7);
      return 0;
    }
    sceKernelCpuResumeIntr(v7);
    return -2147352124;
  }
}


int sceKernelCancelMutex(int a1, int a2, int *a3)
{
  int v3;
  int v4;
  int v5;
  int v7;
  int v9;
  int v10;
  int UIDcontrolBlockWithType;
  int v12;
  int v13;
  int v14;
  u32 *v15;
  int *v16;
  int *v17;
  int v18;
  int v19;
  int v21[4];

  tman_cfc0(13);
  v5 = a2;
  v7 = v4 << 11;
  if ( v3 != 0 && a2 != 0 )
    return -2147352476;
  v9 = sceKernelCpuSuspendIntr(a1, a2);
  v10 = v9;
  if ( (((unsigned int)(a3 + 1) | (unsigned int)a3) & v7 & 0x80000000) != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352365;
  }
  else
  {
    UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x518), v21);
    v13 = v21[0];
    if ( UIDcontrolBlockWithType != 0 )
    {
      sceKernelCpuResumeIntr(v10);
      return -2147352125;
    }
    else
    {
      v14 = *(u16 *)(v21[0] + 22) & 8;
      if ( v7 >= 0 )
        v14 = 8;
      v15 = (u32 *)TMAN_BSS_U32(0x40);
      if ( v14 != 0 )
      {
        v16 = (int *)(v21[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
        if ( v5 < 0 )
          v5 = *(u32 *)(v21[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x518) + 21) + 16);
        v17 = &TMAN_BSS_U32(0x40);
        if ( v5 < 2 || (v12 = *v16, (*v16 & 0x200) != 0) )
        {
          if ( TMAN_BSS_U32(0x48) != NULL )
          {
            ((void (*)(int, int, int, int, int, int *))TMAN_BSS_U32(0x48))(80, v5, 3, a1, v5, a3);
            v13 = v21[0];
          }
          tman_sync_fn_14(v13, v12, v13, v17);
          v19 = sceKernelCallUIDObjFunction(v21[0], -2032580477, v18);
          if ( v5 != 0 )
            tman_sync_fn_11(v21[0], v15, v5);
          if ( a3 != NULL )
            *a3 = v19;
          sceKernelCpuResumeIntr(v10);
          return 0;
        }
        else
        {
          sceKernelCpuResumeIntr(v10);
          return -2147352131;
        }
      }
      else
      {
        sceKernelCpuResumeIntr(v10);
        return -2147352367;
      }
    }
  }
}


int sceKernelReferMutexStatus(int a1, unsigned int *a2)
{
  int v2;
  int v3;
  int v5;
  int v6;
  int v7;
  int UIDcontrolBlockWithType;
  int v10;
  int v11;
  int v12;
  int *v13;
  u32 *v14;
  int *v15;
  int v16;
  int v17;
  int v18;
  unsigned int v19;
  u32 v21[4];

  v5 = (unsigned int)(a2 + 14) | (unsigned int)a2;
  v6 = v5 & (v2 << 11);
  v7 = v2 << 11;
  if ( v6 < 0 )
    return -2147352365;
  v10 = sceKernelCpuSuspendIntr(a1, v5);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x518), v21);
  v11 = v21[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v10);
    return -2147352125;
  }
  else
  {
    v12 = *(u16 *)(v21[0] + 22) & 1;
    if ( v7 >= 0 )
      v12 = 1;
    if ( v12 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int, int, int, unsigned int *))TMAN_BSS_U32(0x48))(81, v3, 2, a1, a2);
        v11 = v21[0];
      }
      v13 = (int *)&TMAN_BSS_U8(0x6DC);
      v14 = (u32 *)(v11 + 4 * *(u8 *)(TMAN_BSS_U32(0x518) + 21));
      v15 = (int *)(v11 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
      do
      {
        ++v13;
        v13[13] = 0;
      }
      while ( v13 != &TMAN_BSS_U32(0x714) );
      v16 = *(u32 *)(v11 + 16);
      TMAN_BSS_U32(0x714) = 56;
      if ( v16 != 0 )
        strncpy(&TMAN_BSS_U32(0x718), v16, 31);
      v17 = v14[6];
      TMAN_BSS_U32(0x738) = *v15;
      v18 = -1;
      TMAN_BSS_U32(0x748) = v15[2];
      if ( v17 != 0 )
      {
        v18 = -2;
        if ( v17 != -1 )
          v18 = *(u32 *)(v17 + 8);
      }
      TMAN_BSS_U32(0x744) = v18;
      v19 = *a2;
      TMAN_BSS_U32(0x740) = v14[5];
      if ( TMAN_BSS_U32(0x714) < v19 )
        v19 = TMAN_BSS_U32(0x714);
      TMAN_BSS_U32(0x73C) = v14[4];
      memcpy(a2, &TMAN_BSS_U32(0x714), v19);
      sceKernelCpuResumeIntr(v10);
      return 0;
    }
    else
    {
      sceKernelCpuResumeIntr(v10);
      return -2147352367;
    }
  }
}


int tman_sync_fn_13()
{
  sceKernelCreateUIDtypeInherit("WaitQ", "Mutex", 48, g_uid_mutex_funcs, 0, &TMAN_BSS_U32(0x518));
  return 0;
}


void tman_sync_fn_14(int a1)
{
  int v1;
  int *v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;

  v1 = TMAN_BSS_U32(0x518);
  v2 = (int *)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x518) + 21));
  v3 = v2[6];
  v2[5] = 0;
  v2[6] = 0;
  if ( v3 != 0 && v3 != -1 )
  {
    v4 = *v2;
    v5 = v3;
    *(u32 *)(v4 + 4) = v2[1];
    *(u32 *)v2[1] = v4;
    v6 = *(u32 *)(v3 + 168);
    if ( v6 == v3 + 168
      || (*(u32 *)(v6 - 4 * *(u8 *)(v1 + 21) + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21)) & 0x400) == 0 )
    {
      v9 = *(s16 *)(v3 + 186);
      *(u32 *)(v3 + 188) = 127;
    }
    else
    {
      v7 = *(u32 *)(v6 + 28);
      v8 = *(s16 *)(v3 + 186);
      v9 = v7;
      *(u32 *)(v3 + 188) = v7;
      if ( v7 >= v8 )
      {
        v5 = v3;
        v9 = v8;
      }
    }
    tman_thread_fn_11(v5, v9, 0);
  }
}


int tman_sync_fn_15(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_sync_fn_16(int a1, int a2, int a3, int a4)
{
  tman_sync_fn_14(a1);
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return 0;
}


int tman_sync_fn_17(int a1)
{
  int v2;
  int v3;
  int v4;
  int result;
  int *v6;
  int i;
  int v8;
  int v9;

  v2 = 0;
  v3 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  v4 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x518) + 21);
  result = 0;
  if ( *(int *)(v3 + 8) > 0 )
  {
    v6 = *(int **)(v3 + 12);
    if ( v6 == (int *)(v3 + 12) )
    {
      return 0;
    }
    else
    {
      for ( i = TMAN_BSS_U32(0x70); ; i = TMAN_BSS_U32(0x70) )
      {
        if ( i == 0 )
        {
          v9 = *(u32 *)(v4 + 24);
          goto LABEL_8;
        }
        v8 = v6[5];
        if ( v8 >= 0 && (v8 & 0x8000000) == 0 )
          break;
        v6 = (int *)*v6;
        if ( v6 == (int *)(v3 + 12) )
          return 0;
      }
      v9 = *(u32 *)(v4 + 24);
LABEL_8:
      if ( v9 != 0 )
      {
        return 0;
      }
      else
      {
        if ( tman_sched_fn_6(v6) > 0 )
          v2 = 1;
        tman_sync_fn_11(a1, v6, v6[24]);
        return v2;
      }
    }
  }
  return result;
}


int sceKernelCreateLwMutex(int *a1, int a2, int a3, int a4, int *a5, int a6)
{
  int v6;
  int v7;
  int v9;
  int v14;
  int v15;
  int v16;
  int v18;
  int v19;
  int UID;
  u32 *v21;
  int v22;
  int v23[4];

  v7 = v6 << 11;
  v9 = (v6 << 11) & (unsigned int)a1;
  v14 = v6 << 11;
  if ( v9 < 0 )
    return -2147352365;
  v15 = v7 & a2;
  if ( (v7 & a2) < 0 )
    return -2147352365;
  v16 = ((unsigned int)(a5 + 1) | (unsigned int)a5) & v7;
  if ( v16 < 0 || ((u8)a1 & 3) != 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a6 != 0 )
    return -2147352476;
  if ( (a3 & 0xFFFFFC00) != 0 )
    return -2147352175;
  if ( a4 < 0 || a4 >= 2 && (a3 & 0x200) == 0 )
    return -2147352131;
  v18 = sceKernelCpuSuspendIntr(v15, v16);
  TMAN_BSS_U32(0x714) = 0;
  if ( a5 != NULL )
  {
    v19 = *a5;
    if ( (unsigned int)*a5 >= 5 )
      v19 = 4;
    memcpy(&TMAN_BSS_U32(0x714), a5, v19);
  }
  UID = sceKernelCreateUID(TMAN_BSS_U32(0x51C), a2, (u8)(a3 | (v14 >> 31)), v23);
  if ( UID != 0 )
  {
    sceKernelCpuResumeIntr(v18);
    return UID;
  }
  else
  {
    v21 = (u32 *)(v23[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x51C) + 21));
    v22 = *(u32 *)(v23[0] + 12) ^ TMAN_BSS_U32(0x844);
    *(u32 *)(v23[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21)) = a3;
    v21[4] = a5;
    v21[5] = a4;
    v21[2] = v22;
    v21[3] = a1;
    a1[2] = a3;
    *a1 = a4;
    a1[1] = 0;
    a1[3] = 0;
    a1[4] = v22;
    a1[5] = 0;
    a1[6] = 0;
    a1[7] = 0;
    if ( a4 > 0 )
      a1[1] = *(u32 *)(TMAN_BSS_U32(0x40) + 8);
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((int (*)())TMAN_BSS_U32(0x48))(82, a1, 1, v22);
    sceKernelCpuResumeIntr(v18);
    return 0;
  }
}


int sceKernelDeleteLwMutex(int a1)
{
  int v1;
  int v2;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  u32 v10[4];

  v2 = (v1 << 11) & a1;
  v4 = v1 << 11;
  if ( v2 < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a1 != 0 )
    return -2147352476;
  v5 = sceKernelCpuSuspendIntr(0, -2147352576);
  v6 = *(u32 *)(a1 + 16);
  v7 = v5;
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v6, TMAN_BSS_U32(0x51C), v10) != 0
    || *(u32 *)(v10[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x51C) + 21) + 12) != a1 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352118;
  }
  else
  {
    v8 = *(u16 *)(v10[0] + 22) & 0x10;
    if ( v4 >= 0 )
      v8 = 16;
    if ( v8 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((int (*)())TMAN_BSS_U32(0x48))(83, a1, 1, v6);
      sceKernelDeleteUID(v6);
      *(u32 *)(a1 + 16) = -1;
      *(u32 *)(a1 + 4) = -1;
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


int sceKernelUnlockLwMutex(unsigned int a1, int a2)
{
  int v2;
  int v3;
  int *v5;
  int v6;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 *v12;
  int v14;
  int *v15;
  int v16;
  int v17;
  u32 v18[4];

  v3 = (v2 << 11) & a1;
  v5 = NULL;
  v6 = v2 << 11;
  if ( v3 < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a2 != 0 )
    return -2147352476;
  v8 = sceKernelCpuSuspendIntr(a1, 0);
  v9 = *(u32 *)(a1 + 16);
  v10 = v8;
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v9, TMAN_BSS_U32(0x51C), v18) != 0
    || *(u32 *)(v18[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x51C) + 21) + 12) != a1 )
  {
    sceKernelCpuResumeIntr(v10);
    return -2147352118;
  }
  else
  {
    v11 = *(u16 *)(v18[0] + 22) & 2;
    if ( v6 >= 0 )
      v11 = 2;
    if ( v11 != 0 )
    {
      v12 = (u32 *)(v18[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
      if ( a2 > 0 && (a2 < 2 || (*v12 & 0x200) != 0) )
      {
        if ( *(u32 *)(a1 + 4) == *(u32 *)(TMAN_BSS_U32(0x40) + 8) )
        {
          if ( TMAN_BSS_U32(0x48) != NULL )
            ((void (*)(int, unsigned int, int, int, int))TMAN_BSS_U32(0x48))(84, a1, 2, v9, a2);
          v14 = *(u32 *)a1 - a2;
          if ( *(u32 *)a1 < a2 )
          {
            sceKernelCpuResumeIntr(v10);
            return -2147352114;
          }
          else
          {
            *(u32 *)a1 = v14;
            if ( v14 <= 0 )
            {
              v15 = (int *)v12[3];
              *(u32 *)(a1 + 4) = 0;
              v16 = 0;
              *(u32 *)a1 = 0;
              if ( v15 != v12 + 3 )
              {
                v17 = v15[2];
                *(u32 *)a1 = v15[24];
                *(u32 *)(a1 + 4) = v17;
                if ( tman_sched_fn_6(v15) > 0 )
                {
                  v5 = v15;
                  v16 = 1;
                }
              }
              if ( v16 == 1 )
              {
                tman_thread_fn_15(v5, v5[4]);
                tman_thread_fn_16(v5, v10);
              }
            }
            sceKernelCpuResumeIntr(v10);
            return 0;
          }
        }
        else
        {
          sceKernelCpuResumeIntr(v10);
          return -2147352116;
        }
      }
      else
      {
        sceKernelCpuResumeIntr(v10);
        return -2147352131;
      }
    }
    else
    {
      sceKernelCpuResumeIntr(v10);
      return -2147352367;
    }
  }
}


void sceKernelLockLwMutex()
{
  JUMPOUT(0x7DE8);
}


int sceKernelLockLwMutexCB(int *a1, int a2, int a3, int a4, int a5, int a6)
{
  int v6;
  int v7;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  u32 *v24;
  int v25;
  u32 *v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  u8 v34[16];
  int v35;
  int v36;
  int v37;
  int v38;
  int v39;

  v39 = v7;
  v11 = (unsigned int)(a1 + 8) | (unsigned int)a1;
  v12 = v6 << 11;
  v13 = v11 & (v6 << 11);
  v14 = v6 << 11;
  if ( v13 < 0 )
    return -2147352365;
  v15 = ((a3 + 4) | a3) & v12;
  if ( v15 < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a6 != 0 )
    return -2147352476;
  v17 = sceKernelCpuSuspendIntr(v15, v11);
  v19 = v17;
  if ( v17 == 0 || TMAN_BSS_U32(0x4C) != 0 )
  {
    sceKernelCpuResumeIntr(v17);
    return -2147352153;
  }
  tman_thread_fn_17(v18);
  v36 = a1[4];
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v36, TMAN_BSS_U32(0x51C), &v35) != 0
    || *(int **)(v35 + 4 * *(u8 *)(TMAN_BSS_U32(0x51C) + 21) + 12) != a1 )
  {
    sceKernelCpuResumeIntr(v19);
    return -2147352118;
  }
  else
  {
    v21 = *(u16 *)(v35 + 22) & 2;
    if ( v14 >= 0 )
      v21 = 2;
    v22 = 0x20000u;
    if ( v21 != 0 )
    {
      v23 = TMAN_BSS_U32(0x508);
      v24 = (u32 *)TMAN_BSS_U32(0x40);
      v25 = *(u8 *)(TMAN_BSS_U32(0x508) + 21);
      v26 = (u32 *)(v35 + 4 * v25);
      if ( a2 <= 0 || (v27 = a2 < 2, a2 >= 2) && (v20 = *v26 & 0x200) == 0 )
      {
        sceKernelCpuResumeIntr(v19);
        return -2147352131;
      }
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((void (*)(int, int *, int, int, int))TMAN_BSS_U32(0x48))(86, a1, 2, v36, a2);
      if ( a1[1] != v24[2] )
      {
        if ( a3 != 0 )
          tman_mem_fn_34(v34, *(u32 *)a3);
        v28 = 1;
        v29 = -2147352149;
        v37 = 1;
        v38 = -2147352149;
        while ( 1 )
        {
          if ( v24[39] != 0 )
          {
            tman_sched_fn_21(v19, v29, v27, v28, v25, v23, v22, v20);
            if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v36, TMAN_BSS_U32(0x51C), &v35) != 0 )
            {
              sceKernelCpuResumeIntr(v19);
              return -2147352139;
            }
            v30 = v26[2];
          }
          else
          {
            v30 = v26[2];
          }
          if ( v30 > 0 )
          {
            if ( (*v26 & 0x100) == 0 )
            {
              v24[24] = a2;
              goto LABEL_33;
            }
            if ( v24[4] >= *(u32 *)(v26[3] + 16) )
            {
              v24[24] = a2;
              goto LABEL_33;
            }
          }
          if ( a1[1] == 0 )
          {
            v31 = v24[2];
            v32 = 0;
            *a1 = a2;
            a1[1] = v31;
LABEL_34:
            sceKernelCpuResumeIntr(v19);
            return v32;
          }
          v24[24] = a2;
LABEL_33:
          v24[38] = 1;
          v32 = tman_sched_fn_2(v35, 13, *v26, (int)v34, a3, a1 + 3);
          v28 = v32 ^ v38;
          v29 = v32 == v38;
          if ( (v37 & v29) == 0 )
            goto LABEL_34;
        }
      }
      if ( (*v26 & 0x200) != 0 )
      {
        v33 = a2 + *a1;
        if ( v33 < 0 )
        {
          sceKernelCpuResumeIntr(v19);
          return -2147352115;
        }
        else
        {
          *a1 = v33;
          sceKernelCpuResumeIntr(v19);
          return 0;
        }
      }
      else
      {
        sceKernelCpuResumeIntr(v19);
        return -2147352113;
      }
    }
    else
    {
      sceKernelCpuResumeIntr(v19);
      return -2147352367;
    }
  }
}


int sceKernelTryLockLwMutex(int *a1, int a2)
{
  int v2;
  int v3;
  int v5;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 *v12;
  void (*v13)(int, int *, int, int, int);
  int v15;
  int v16;
  int v17;
  int v18;
  int v19[4];

  v3 = (v2 << 11) & (unsigned int)a1;
  v5 = v2 << 11;
  if ( v3 < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a2 != 0 )
    return -2147352476;
  v7 = sceKernelCpuSuspendIntr(a1, 0);
  v8 = a1[4];
  v9 = v7;
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v8, TMAN_BSS_U32(0x51C), v19) != 0
    || *(int **)(v19[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x51C) + 21) + 12) != a1 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352118;
  }
  v10 = *(u16 *)(v19[0] + 22) & 2;
  if ( v5 >= 0 )
    v10 = 2;
  if ( v10 == 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352367;
  }
  v11 = TMAN_BSS_U32(0x40);
  v12 = (u32 *)(v19[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
  if ( a2 <= 0 )
    goto LABEL_12;
  if ( a2 >= 2 )
  {
    if ( (*v12 & 0x200) != 0 )
    {
      v13 = (void (*)(int, int *, int, int, int))TMAN_BSS_U32(0x48);
      goto LABEL_14;
    }
LABEL_12:
    sceKernelCpuResumeIntr(v9);
    return -2147352131;
  }
  v13 = (void (*)(int, int *, int, int, int))TMAN_BSS_U32(0x48);
LABEL_14:
  if ( v13 != NULL )
  {
    v13(87, a1, 2, v8, a2);
    v18 = *(u32 *)(v11 + 8);
  }
  else
  {
    v18 = *(u32 *)(TMAN_BSS_U32(0x40) + 8);
  }
  v15 = a1[1];
  if ( v15 == v18 )
  {
    if ( (*v12 & 0x200) != 0 )
    {
      v17 = a2 + *a1;
      v16 = v9;
      if ( v17 >= 0 )
      {
        *a1 = v17;
        goto LABEL_20;
      }
      sceKernelCpuResumeIntr(v9);
      return -2147352115;
    }
    else
    {
      sceKernelCpuResumeIntr(v9);
      return -2147352113;
    }
  }
  else
  {
    v16 = v9;
    if ( v15 == 0 )
    {
      *a1 = a2;
      a1[1] = v18;
LABEL_20:
      sceKernelCpuResumeIntr(v16);
      return 0;
    }
    sceKernelCpuResumeIntr(v9);
    return -2147352117;
  }
}


int sceKernelReferLwMutexStatusByID(int a1, unsigned int *a2)
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
  int v13;
  int *v14;
  int *v15;
  int v16;
  int v17;
  int v18;
  unsigned int v19;
  u32 v21[4];

  v4 = (unsigned int)(a2 + 16) | (unsigned int)a2;
  v5 = v4 & (v2 << 11);
  v6 = v2 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v9 = sceKernelCpuSuspendIntr(a1, v4);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x51C), v21);
  v10 = v21[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352118;
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
        ((void (*)(int, u32, int, int, unsigned int *))TMAN_BSS_U32(0x48))(89, v21[0], 2, a1, a2);
        v10 = v21[0];
      }
      v12 = &TMAN_BSS_U32(0x6D4);
      v13 = v10 + 4 * *(u8 *)(TMAN_BSS_U32(0x51C) + 21);
      v14 = *(int **)(v13 + 12);
      v15 = (int *)(v10 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
      do
      {
        ++v12;
        v12[15] = 0;
      }
      while ( v12 != &TMAN_BSS_U32(0x714) );
      v16 = *(u32 *)(v10 + 16);
      TMAN_BSS_U32(0x714) = 64;
      if ( v16 != 0 )
        strncpy(&TMAN_BSS_U32(0x718), v16, 31);
      TMAN_BSS_U32(0x738) = *v15;
      v17 = *(u32 *)(v10 + 12);
      TMAN_BSS_U32(0x740) = (int)v14;
      TMAN_BSS_U32(0x73C) = v17 ^ TMAN_BSS_U32(0x844);
      TMAN_BSS_U32(0x750) = v15[2];
      v18 = v14[1];
      if ( v18 == 0 )
        v18 = -1;
      TMAN_BSS_U32(0x74C) = v18;
      v19 = *a2;
      TMAN_BSS_U32(0x748) = *v14;
      if ( TMAN_BSS_U32(0x714) < v19 )
        v19 = TMAN_BSS_U32(0x714);
      TMAN_BSS_U32(0x744) = *(u32 *)(v13 + 20);
      memcpy(a2, &TMAN_BSS_U32(0x714), v19);
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


int tman_sync_fn_18()
{
  sceKernelCreateUIDtypeInherit("WaitQ", "LwMutex", 48, g_uid_lwmutex_funcs, 0, &TMAN_BSS_U32(0x51C));
  return 0;
}


int tman_sync_fn_19(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_sync_fn_20(int a1, int a2, int a3, int a4)
{
  u32 *v4;

  v4 = *(u32 **)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x51C) + 21) + 12);
  *v4 = 0;
  v4[1] = 0;
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return 0;
}


int tman_sync_fn_21(int a1)
{
  int v2;
  int v3;

  v2 = *(u32 *)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x51C) + 21) + 12);
  v3 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  --*(u32 *)(v2 + 12);
  --*(u32 *)(v3 + 8);
  return 0;
}


int tman_sync_fn_22(int a1)
{
  int v1;
  int v2;
  int result;
  int v4;
  u32 *v5;
  int v6;
  int v7;

  v1 = false;
  v2 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  result = false;
  if ( *(int *)(v2 + 8) > 0 )
  {
    v4 = *(u32 *)(v2 + 12);
    result = false;
    if ( v4 != v2 + 12 )
    {
      v5 = *(u32 **)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x51C) + 21) + 12);
      if ( v5[1] == 0 )
      {
        v6 = tman_sched_fn_6(*(int **)(v2 + 12));
        v7 = *(u32 *)(v4 + 96);
        v1 = v6 > 0;
        v5[1] = *(u32 *)(v4 + 8);
        *v5 = v7;
      }
      return v1;
    }
  }
  return result;
}


int sceKernelCreateMbx(int a1, int a2, int a3, int a4)
{
  int v4;
  int v5;
  int v7;
  int v8;
  int v11;
  int v13;
  int UID;
  int v15;
  int v16;
  void (*v17)(int, u32, int, int);
  u32 *v18;
  int v19;
  u32 v20[4];

  v5 = v4 << 11;
  v7 = (v4 << 11) & a1;
  v8 = v4 << 11;
  if ( v7 < 0 )
    return -2147352365;
  v11 = (a3 + 4) | a3;
  if ( (v11 & v5) < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a4 != 0 )
    return -2147352476;
  if ( (a2 & 0xFFFFFA00) != 0 )
    return -2147352175;
  v13 = sceKernelCpuSuspendIntr(v11 & v5, v11);
  UID = sceKernelCreateUID(TMAN_BSS_U32(0x520), a1, (u8)(a2 | (v8 >> 31)), v20);
  if ( UID != 0 )
  {
    sceKernelCpuResumeIntr(v13);
    return UID;
  }
  else
  {
    v15 = *(u32 *)(v20[0] + 12);
    v16 = TMAN_BSS_U32(0x844);
    v17 = (void (*)(int, u32, int, int))TMAN_BSS_U32(0x48);
    v18 = (u32 *)(v20[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x520) + 21));
    *(u32 *)(v20[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21)) = a2;
    v19 = v15 ^ v16;
    *v18 = a3;
    if ( v17 != NULL )
      v17(90, 0, 1, v15 ^ v16);
    sceKernelCpuResumeIntr(v13);
    return v19;
  }
}


int sceKernelDeleteMbx(int a1, int a2)
{
  int v2;
  int v3;
  int v5;
  int v6;
  int v7;
  u32 v9[4];

  v3 = -2147352476;
  tman_cfc0(13);
  v5 = v2 << 11;
  if ( a1 == 0 )
  {
    v6 = sceKernelCpuSuspendIntr(0, a2);
    if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(0, TMAN_BSS_U32(0x520), v9) != 0 )
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352165;
    }
    else
    {
      v7 = *(u16 *)(v9[0] + 22) & 0x10;
      if ( v5 >= 0 )
        v7 = 16;
      if ( v7 != 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((int (*)())TMAN_BSS_U32(0x48))(91, a1, 1, a1);
        sceKernelDeleteUID(0);
        sceKernelCpuResumeIntr(v6);
        return 0;
      }
      else
      {
        sceKernelCpuResumeIntr(v6);
        return -2147352367;
      }
    }
  }
  return v3;
}


int sceKernelSendMbx(int a1, int a2)
{
  int v2;
  int v3;
  int v5;
  int v6;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  u32 *v14;
  int v16;
  int v17[4];

  v5 = (a2 + 8) | a2;
  v6 = v5 & (v3 << 11);
  tman_cfc0(13);
  v8 = v3 << 11;
  if ( v6 < 0 )
    return -2147352365;
  v9 = sceKernelCpuSuspendIntr(a1, v5);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x520), v17) != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352165;
  }
  else
  {
    v10 = *(u16 *)(v17[0] + 22) & 2;
    if ( v8 >= 0 )
      v10 = 2;
    if ( v10 != 0 )
    {
      v11 = *(u8 *)(TMAN_BSS_U32(0x508) + 21);
      v12 = v17[0] + 4 * v11;
      v13 = v17[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x520) + 21);
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((void (*)(int, int, int, int, int))TMAN_BSS_U32(0x48))(92, v17[0] + 4 * v11, 2, a1, a2);
      if ( *(int *)(v12 + 8) > 0 )
      {
        v14 = *(u32 **)(*(u32 *)(v12 + 12) + 96);
        if ( v14 != NULL )
          *v14 = a2;
        tman_sched_fn_8(v17[0], 0, v2, v9);
        goto LABEL_12;
      }
      v16 = tman_sync_fn_24(v13, v12, a2);
      if ( v16 == 0 )
      {
LABEL_12:
        sceKernelCpuResumeIntr(v9);
        return 0;
      }
      sceKernelCpuResumeIntr(v9);
      return v16;
    }
    else
    {
      sceKernelCpuResumeIntr(v9);
      return -2147352367;
    }
  }
}


int sceKernelReceiveMbx(int a1, unsigned int a2, int a3, int a4, int a5, int a6)
{
  int v6;
  int v7;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  u32 **v24;
  u32 *v25;
  u32 *v26;
  void (*v27)(u32, int, int, const char *, ...);
  int v28; // dc
  u8 v29[16];
  int v30;
  int v31;
  int v32;
  u32 *v33;
  int v34;

  v34 = v7;
  v31 = a1;
  v10 = (a2 + 4) | a2;
  v11 = v6 << 11;
  v12 = v10 & (v6 << 11);
  v13 = v6 << 11;
  if ( v12 < 0 )
    return -2147352365;
  v14 = ((a3 + 4) | a3) & v11;
  if ( v14 < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a6 != 0 )
    return -2147352476;
  v16 = sceKernelCpuSuspendIntr(v14, v10);
  v18 = v16;
  if ( v16 == 0 || (v19 = (int)&TMAN_BSS_U32(0x40), TMAN_BSS_U32(0x4C) != 0) )
  {
    sceKernelCpuResumeIntr(v16);
    return -2147352153;
  }
  tman_thread_fn_17(v17);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v31, TMAN_BSS_U32(0x520), &v30) != 0 )
  {
    sceKernelCpuResumeIntr(v18);
    return -2147352165;
  }
  else
  {
    v20 = *(u16 *)(v30 + 22) & 4;
    if ( v13 >= 0 )
      v20 = 4;
    v21 = TMAN_BSS_U32(0x40);
    if ( v20 != 0 )
    {
      v22 = *(u8 *)(TMAN_BSS_U32(0x520) + 21);
      v33 = (u32 *)(v30 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
      v32 = v30 + 4 * v22;
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((void (*)(int, int *, int, int, unsigned int))TMAN_BSS_U32(0x48))(93, &TMAN_BSS_U32(0x40), 2, v31, a2);
      if ( a3 != 0 )
        tman_mem_fn_34(v29, *(u32 *)a3);
      v23 = *(u32 *)(v32 + 4);
      if ( v23 > 0 )
      {
        v24 = *(u32 ***)(v32 + 8);
        v25 = *v24;
        if ( *v24 != NULL && (v13 & (unsigned int)*v24 & 0x80000000) == 0 )
        {
          v26 = (u32 *)*v25;
          if ( *v25 != 0 && (v13 & *v25) >= 0 )
          {
            if ( v25 == v26 )
            {
              v19 = -2147352119;
              if ( v23 != 1 )
                goto LABEL_17;
              *(u32 *)(v32 + 8) = 0;
            }
            else
            {
              *v24 = v26;
            }
            v27 = (void (*)(u32, int, int, const char *, ...))TMAN_BSS_U32(0x48);
            v28 = TMAN_BSS_U32(0x48) != NULL;
            *(u32 *)(v32 + 4) = v23 - 1;
            if ( v28 )
              v27(0, v19, 2, " recvmsg = 0x%x", v25);
            v19 = 0;
            *(u32 *)a2 = v25;
            goto LABEL_17;
          }
        }
        v19 = -2147352365;
      }
      else
      {
        *(u32 *)(v21 + 96) = a2;
        *(u32 *)(v21 + 152) = 0;
        v19 = tman_sched_fn_5(v30, 5, *v33, (int)v29, a3);
      }
LABEL_17:
      sceKernelCpuResumeIntr(v18);
      return v19;
    }
    sceKernelCpuResumeIntr(v18);
    return -2147352367;
  }
}


int sceKernelPollMbx(int a1, unsigned int a2)
{
  int v2;
  int v4;
  int v5;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 **v12;
  u32 *v13;
  u32 *v14;
  void (*v16)(u32, u32 *, int, const char *, ...);
  u32 v17[4];

  v4 = (a2 + 4) | a2;
  v5 = v4 & (v2 << 11);
  v7 = v2 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v8 = sceKernelCpuSuspendIntr(a1, v4);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x520), v17) != 0 )
  {
    sceKernelCpuResumeIntr(v8);
    return -2147352165;
  }
  else
  {
    v9 = *(u16 *)(v17[0] + 22) & 4;
    if ( v7 >= 0 )
      v9 = 4;
    if ( v9 != 0 )
    {
      v10 = v17[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x520) + 21);
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((void (*)(int, int *, int, int, unsigned int))TMAN_BSS_U32(0x48))(95, &TMAN_BSS_U32(0x40), 2, a1, a2);
      v11 = *(u32 *)(v10 + 4);
      if ( v11 <= 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((int (*)())TMAN_BSS_U32(0x48))(0, &TMAN_BSS_U32(0x40), 1, "sceKernelPollMbx nomsg");
        sceKernelCpuResumeIntr(v8);
        return -2147352142;
      }
      else
      {
        v12 = *(u32 ***)(v10 + 8);
        v13 = *v12;
        if ( *v12 == NULL
          || (v7 & (unsigned int)*v12 & 0x80000000) != 0
          || (v14 = (u32 *)*v13, *v13 == 0)
          || (v7 & *v13) < 0 )
        {
          sceKernelCpuResumeIntr(v8);
          return -2147352365;
        }
        if ( v13 == v14 )
        {
          if ( v11 != 1 )
          {
            sceKernelCpuResumeIntr(v8);
            return -2147352119;
          }
          *(u32 *)(v10 + 8) = 0;
        }
        else
        {
          *v12 = v14;
        }
        v16 = (void (*)(u32, u32 *, int, const char *, ...))TMAN_BSS_U32(0x48);
        *(u32 *)(v10 + 4) = v11 - 1;
        if ( v16 != NULL )
          v16(0, v13, 2, " recvmsg = 0x%x", v13);
        *(u32 *)a2 = v13;
        sceKernelCpuResumeIntr(v8);
        return 0;
      }
    }
    else
    {
      sceKernelCpuResumeIntr(v8);
      return -2147352367;
    }
  }
}


int sceKernelCancelReceiveMbx(int a1, int *a2)
{
  int v2;
  int v4;
  int v5;
  int v7;
  int UIDcontrolBlockWithType;
  int v9;
  int v10;
  int v11;
  int v12;
  u32 v14[4];

  v4 = (unsigned int)(a2 + 1) | (unsigned int)a2;
  v5 = v4 & (v2 << 11);
  v7 = v2 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v9 = sceKernelCpuSuspendIntr(a1, v4);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x520), v14);
  v10 = v14[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352165;
  }
  else
  {
    v11 = *(u16 *)(v14[0] + 22) & 8;
    if ( v7 >= 0 )
      v11 = 8;
    if ( v11 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int *, int, int, int *))TMAN_BSS_U32(0x48))(96, a2, 2, a1, a2);
        v10 = v14[0];
      }
      v12 = sceKernelCallUIDObjFunction(v10, -2032580477, v10);
      if ( a2 != NULL )
        *a2 = v12;
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


int sceKernelReferMbxStatus(int a1, int *a2)
{
  int v2;
  int v3;
  int v5;
  int v6;
  int v7;
  int UIDcontrolBlockWithType;
  int v10;
  int v11;
  int v12;
  int *v13;
  int v14;
  int *v15;
  int v16;
  int *v17;
  int v18;
  u32 v20[4];

  v5 = (unsigned int)(a2 + 13) | (unsigned int)a2;
  v6 = v5 & (v2 << 11);
  v7 = v2 << 11;
  if ( v6 < 0 )
    return -2147352365;
  v10 = sceKernelCpuSuspendIntr(a1, v5);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x520), v20);
  v11 = v20[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v10);
    return -2147352165;
  }
  else
  {
    v12 = *(u16 *)(v20[0] + 22) & 1;
    if ( v7 >= 0 )
      v12 = 1;
    if ( v12 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int, int, int, int *))TMAN_BSS_U32(0x48))(97, v3, 2, a1, a2);
        v11 = v20[0];
      }
      v13 = (int *)&TMAN_BSS_U8(0x6E0);
      v14 = v11 + 4 * *(u8 *)(TMAN_BSS_U32(0x520) + 21);
      v15 = (int *)(v11 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
      do
      {
        ++v13;
        v13[12] = 0;
      }
      while ( v13 != &TMAN_BSS_U32(0x714) );
      v16 = *(u32 *)(v11 + 16);
      TMAN_BSS_U32(0x714) = 52;
      if ( v16 != 0 )
        strncpy(&TMAN_BSS_U32(0x718), v16, 31);
      v17 = *(int **)(v14 + 8);
      TMAN_BSS_U32(0x738) = *v15;
      TMAN_BSS_U32(0x73C) = v15[2];
      TMAN_BSS_U32(0x740) = *(u32 *)(v14 + 4);
      if ( v17 != NULL )
        TMAN_BSS_U32(0x744) = *v17;
      v18 = *a2;
      if ( TMAN_BSS_U32(0x714) < (unsigned int)*a2 )
        v18 = TMAN_BSS_U32(0x714);
      memcpy(a2, &TMAN_BSS_U32(0x714), v18);
      sceKernelCpuResumeIntr(v10);
      return 0;
    }
    else
    {
      sceKernelCpuResumeIntr(v10);
      return -2147352367;
    }
  }
}


int tman_sync_fn_23()
{
  sceKernelCreateUIDtypeInherit("WaitQ", "Mbx", 48, g_uid_mbx_funcs, 0, &TMAN_BSS_U32(0x520));
  return 0;
}


int tman_sync_fn_24(int a1, u32 *a2, unsigned int a3)
{
  int v3;
  int *v4;
  int v5;
  unsigned int *v7;
  int v8;
  unsigned int v9;

  v4 = *(int **)(a1 + 8);
  if ( v4 == NULL )
  {
    *(u32 *)(a1 + 8) = a3;
    *(u32 *)a3 = a3;
LABEL_3:
    v5 = 0;
    ++*(u32 *)(a1 + 4);
    return v5;
  }
  v5 = -2147352119;
  if ( v4 == (int *)a3 )
    return v5;
  v7 = *(unsigned int **)(a1 + 8);
  if ( (*a2 & 0x400) != 0 )
  {
    while ( 1 )
    {
      v9 = *v7;
      v5 = -2147352119;
      if ( *v7 == a3 )
        break;
      if ( v9 == 0 || (v3 & *v7 & 0x80000000) != 0 )
        return -2147352365;
      if ( *(u8 *)(a3 + 4) < (unsigned int)*(u8 *)(v9 + 4) )
        goto LABEL_18;
      v7 = (unsigned int *)*v7;
      if ( (int *)v9 == v4 )
      {
        *(u32 *)(a1 + 8) = a3;
        v9 = *(u32 *)v9;
LABEL_18:
        v5 = -2147352365;
        if ( (v3 & v9 & 0x80000000) != 0 )
          return v5;
        *(u32 *)a3 = v9;
        *v7 = a3;
        goto LABEL_3;
      }
    }
  }
  else
  {
    v8 = *v4;
    if ( *v4 == 0 || (v3 & v8) < 0 )
      return -2147352365;
    v5 = -2147352119;
    if ( v8 != a3 )
    {
      *(u32 *)a3 = v8;
      **(u32 **)(a1 + 8) = a3;
      *(u32 *)(a1 + 8) = a3;
      goto LABEL_3;
    }
  }
  return v5;
}


int tman_sync_fn_25(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_sync_fn_26(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return 0;
}


int sceKernelCreateMsgPipe(int a1, int a2, int a3, int a4, int a5)
{
  int v5;
  int v6;
  int v8;
  int BlockHeadAddr;
  int v14;
  int v15;
  int MemoryPartitionInfo;
  int v18;
  int v19;
  int v20;
  void (*v21)(int, u32, int, int);
  u32 *v22;
  int v23;
  int v24;
  int v25[3];
  int v26;
  int v27;
  int v28;

  v6 = v5 << 11;
  v8 = (v5 << 11) & a1;
  BlockHeadAddr = 0;
  v14 = v5 << 11;
  if ( v8 < 0 )
    return -2147352365;
  v15 = (a5 + 4) | a5;
  if ( (v15 & v6) < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a4 != 0 )
    return -2147352476;
  if ( (a3 & 0xFFFFAE00) != 0 )
    return -2147352175;
  v28 = sceKernelCpuSuspendIntr(v15 & v6, v15);
  v25[0] = 16;
  MemoryPartitionInfo = sceKernelQueryMemoryPartitionInfo(a2, v25);
  if ( MemoryPartitionInfo != 0 )
  {
LABEL_13:
    sceKernelCpuResumeIntr(v28);
    return MemoryPartitionInfo;
  }
  if ( v14 >= 0 || (v26 & 3) == 3 )
  {
    v18 = 0;
    if ( a4 != 0 )
    {
      v24 = sceKernelAllocPartitionMemory(a2, a1, (a3 & 0x4000) != 0, a4, 0);
      v18 = v24;
      if ( v24 <= 0 )
      {
        sceKernelCpuResumeIntr(v28);
        return -2147352176;
      }
      BlockHeadAddr = sceKernelGetBlockHeadAddr(v24);
    }
    MemoryPartitionInfo = sceKernelCreateUID(TMAN_BSS_U32(0x524), a1, (u8)(a3 | (v14 >> 31)), &v27);
    if ( MemoryPartitionInfo != 0 )
    {
      if ( v18 > 0 )
        sceKernelFreePartitionMemory(v18);
      goto LABEL_13;
    }
    v19 = *(u32 *)(v27 + 12);
    v20 = TMAN_BSS_U32(0x844);
    v21 = (void (*)(int, u32, int, int))TMAN_BSS_U32(0x48);
    v22 = (u32 *)(v27 + 4 * *(u8 *)(TMAN_BSS_U32(0x524) + 21));
    *(u32 *)(v27 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21)) = a3;
    v23 = v19 ^ v20;
    v22[7] = v18;
    v22[2] = BlockHeadAddr;
    v22[5] = a4;
    *v22 = a5;
    v22[1] = 0;
    v22[4] = BlockHeadAddr;
    v22[3] = BlockHeadAddr;
    v22[6] = 0;
    if ( v21 != NULL )
      v21(98, 0, 1, v23);
    sceKernelCpuResumeIntr(v28);
    return v23;
  }
  else
  {
    sceKernelCpuResumeIntr(v28);
    return -2147352367;
  }
}


int sceKernelDeleteMsgPipe(int a1, int a2)
{
  int v2;
  int v3;
  int v5;
  int v6;
  int v7;
  u32 v9[4];

  v3 = -2147352476;
  tman_cfc0(13);
  v5 = v2 << 11;
  if ( a1 == 0 )
  {
    v6 = sceKernelCpuSuspendIntr(0, a2);
    if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(0, TMAN_BSS_U32(0x524), v9) != 0 )
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352162;
    }
    else
    {
      v7 = *(u16 *)(v9[0] + 22) & 0x10;
      if ( v5 >= 0 )
        v7 = 16;
      if ( v7 != 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((int (*)())TMAN_BSS_U32(0x48))(99, a1, 1, a1);
        sceKernelDeleteUID(0);
        sceKernelCpuResumeIntr(v6);
        return 0;
      }
      else
      {
        sceKernelCpuResumeIntr(v6);
        return -2147352367;
      }
    }
  }
  return v3;
}


void sceKernelSendMsgPipe()
{
  JUMPOUT(0x9930);
}


int sceKernelSendMsgPipeCB(int a1, int a2, unsigned int a3, int a4, unsigned int a5, int a6)
{
  int v6;
  int v7;
  int v8;
  int v9;
  int v11;
  unsigned int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v18;
  int UIDcontrolBlockWithType;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  u32 *v25;
  unsigned int v26;
  int v27;
  u32 *v28;
  void (*v29)(int, int, int, int, int, unsigned int, int);
  int v30;
  u32 *v31;
  int v32;
  int v33;
  u32 *v34;
  int v35;
  int *v36;
  int v37;
  int v38;
  unsigned int v39;
  int v40;
  int v41;
  unsigned int v42;
  int v43;
  int v44;
  int v45;
  int v46;
  unsigned int i;
  int v48;
  int *v49;
  unsigned int v50;
  u32 *v51;
  int v52;
  int v53;
  u8 v54[16];
  int v55;
  int v56;
  int v57;
  int v58;
  u32 *v59;
  int v60;
  int v61;
  unsigned int v62;
  int v63;
  int v64;
  int v65;
  int v66;
  int v67;
  int v68;
  int v69;

  v11 = a2;
  v12 = a3;
  v69 = v9;
  v68 = v8;
  v56 = a1;
  v57 = a4;
  v58 = a6;
  v62 = a3;
  v13 = (a2 + a3) | a2 | a3;
  v14 = v7 << 11;
  v15 = (v7 << 11) & v13;
  v64 = 1;
  v16 = v7 << 11;
  if ( v15 < 0 || (((a5 + 4) | a5) & v14 & 0x80000000) != 0 || (((v58 + 4) | v58) & v14) < 0 )
    return -2147352365;
  if ( (v57 & 0xFFFFFFFE) != 0 )
    return -2147352171;
  tman_cfc0(13);
  if ( v6 != 0 )
    return -2147352476;
  v60 = sceKernelCpuSuspendIntr(v15, v13);
  if ( v60 == 0 || TMAN_BSS_U32(0x4C) != 0 )
  {
    sceKernelCpuResumeIntr(v60);
    return -2147352153;
  }
  tman_thread_fn_17(v18);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(v56, TMAN_BSS_U32(0x524), &v55);
  v20 = v55;
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v60);
    return -2147352162;
  }
  v21 = *(u16 *)(v55 + 22) & 2;
  if ( v16 >= 0 )
    v21 = 2;
  if ( v21 == 0 )
  {
    sceKernelCpuResumeIntr(v60);
    return -2147352367;
  }
  v22 = TMAN_BSS_U32(0x524);
  v23 = TMAN_BSS_U32(0x508);
  v24 = *(u8 *)(TMAN_BSS_U32(0x524) + 21);
  v25 = (u32 *)TMAN_BSS_U32(0x40);
  v26 = *(u32 *)(v55 + 4 * v24 + 20);
  v27 = 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  v59 = (u32 *)(v55 + 4 * v24);
  v28 = (u32 *)(v55 + v27);
  if ( v26 != 0 )
  {
    v20 = v26 < v12;
    if ( v26 < v12 )
    {
      sceKernelCpuResumeIntr(v60);
      return -2147352132;
    }
  }
  if ( v64 == 0 )
  {
    v29 = (void (*)(int, int, int, int, int, unsigned int, int))TMAN_BSS_U32(0x48);
    if ( TMAN_BSS_U32(0x48) == NULL )
    {
      v31 = (u32 *)v58;
      goto LABEL_20;
    }
    v30 = 100;
    goto LABEL_70;
  }
  v29 = (void (*)(int, int, int, int, int, unsigned int, int))TMAN_BSS_U32(0x48);
  v30 = 101;
  if ( TMAN_BSS_U32(0x48) != NULL )
  {
LABEL_70:
    v29(v30, v8, 4, v56, v11, v12, v57);
    v31 = (u32 *)v58;
    goto LABEL_20;
  }
  v31 = (u32 *)v58;
LABEL_20:
  v32 = v57;
  if ( v31 != NULL )
  {
    tman_mem_fn_34(v54, *v31);
    v32 = v57;
  }
  v33 = v57;
  v65 = v32 == 0;
  v66 = v57 != 0;
  v67 = v64 != 0;
  while ( 1 )
  {
    v34 = v59;
    if ( v64 != 0 && v25[39] != 0 )
    {
      tman_sched_fn_21(v60, v33, v20, v32, v64, v24, v23, v22);
      if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v56, TMAN_BSS_U32(0x524), &v55) != 0 )
      {
        sceKernelCpuResumeIntr(v60);
        return -2147352139;
      }
      v34 = v59;
    }
    v61 = 0;
    v35 = v34[1];
    v63 = 0;
    if ( v35 == 1 )
    {
      if ( (*v28 & 0x100) == 0 )
      {
        v63 = 1;
        goto LABEL_65;
      }
      if ( v28[1] != 0 || v25[4] >= *(u32 *)(v28[3] + 16) )
      {
        v63 = 1;
LABEL_65:
        v38 = v57 ^ 1;
        goto LABEL_33;
      }
    }
    v36 = (int *)v28[3];
    if ( v35 == 2 && (int)v28[2] > 0 )
    {
      for ( i = v36[25]; ; i = v36[25] )
      {
        v48 = v36[24];
        v49 = v36;
        v50 = i;
        if ( i >= v12 )
          v50 = v12;
        v36 = (int *)*v36;
        memcpy(v48, v11, v50);
        v51 = (u32 *)v49[27];
        v52 = v49[25] - v50;
        v49[24] += v50;
        v12 -= v50;
        v11 += v50;
        v49[25] = v52;
        if ( v51 != NULL )
          *v51 = v49[26] - v52;
        if ( v49[28] == 1 || v49[25] == 0 )
        {
          v28[1] = 0;
          v53 = tman_sched_fn_6(v49);
          v61 += v53;
        }
        else
        {
          v28[1] = 1;
        }
        if ( v12 == 0 )
          break;
        if ( v59[1] != 2 || (int)v28[2] <= 0 )
          goto LABEL_27;
      }
      v37 = v57;
LABEL_45:
      v38 = v37 ^ 1;
      goto LABEL_33;
    }
LABEL_27:
    v37 = v57;
    if ( v12 == 0 )
      goto LABEL_45;
    v38 = v57 ^ 1;
    v39 = v59[5] - v59[6];
    if ( v39 >= v12 )
      v39 = v12;
    if ( v57 == 1 && v39 != 0 || v65 && v39 == v12 )
    {
      tman_mem_fn_1(v59, v11, v39);
      v12 -= v39;
      v11 += v39;
    }
LABEL_33:
    if ( a5 != 0 )
      *(u32 *)a5 = v62 - v12;
    v40 = v62 != v12;
    if ( (v38 != 0 || v62 != v12) && (v66 || v12 == 0) )
      break;
    v61 = 0;
    v59[1] = 1;
    v25[24] = v11;
    v25[25] = v12;
    v42 = v62;
    v25[27] = a5;
    v25[26] = v42;
    v43 = v57;
    v25[29] = 0;
    v25[28] = v43;
    if ( v63 == 0 )
      v28[1] = v40;
    v44 = v58;
    v45 = v55;
    v25[38] = v64;
    v46 = tman_sched_fn_5(v45, 8, *(u16 *)v28 & 0x100, (int)v54, v44);
    v20 = -2147352576;
    v33 = -2147352149;
    v22 = v67;
    v23 = v46 == 0x800201AB;
    v41 = v46;
    v11 = v25[24];
    v12 = v25[25];
    if ( (v67 & v23) == 0 )
      goto LABEL_41;
  }
  v41 = 0;
LABEL_41:
  if ( v61 != 0 )
  {
    TMAN_BSS_U32(0x44) = 0;
    tman_sched_fn_19(0, v60);
  }
  sceKernelCpuResumeIntr(v60);
  return v41;
}


int sceKernelTrySendMsgPipe(int a1, int a2, unsigned int a3, int a4, unsigned int a5)
{
  int v5;
  int v6;
  int v7;
  unsigned int v8;
  int v9;
  int v10;
  int v11;
  int v14;
  s64 UIDcontrolBlockWithType;
  int v17;
  u32 *v18;
  unsigned int v19;
  u32 *v20;
  int v21;
  unsigned int v22;
  int v23;
  int v24;
  int v25;
  unsigned int v26;
  u32 *v27;
  int *i;
  unsigned int v29;
  u32 *v30;
  int v31;
  int v32;
  u32 *v33;
  int v34;
  int v35;
  u32 *v36;
  int v37;
  int v38;
  unsigned int v39;
  int v40;

  v7 = a2;
  v8 = a3;
  v9 = v5 << 11;
  v10 = (v5 << 11) & ((a2 + a3) | a2 | a3);
  v11 = v5 << 11;
  v36 = (u32 *)a5;
  v38 = 0;
  v39 = a3;
  if ( v10 < 0 )
    return -2147352365;
  v14 = ((a5 + 4) | a5) & v9;
  if ( v14 < 0 )
    return -2147352365;
  if ( (a4 & 0xFFFFFFFE) != 0 )
    return -2147352171;
  v37 = sceKernelCpuSuspendIntr(v14, v10);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x524), &v35);
  tman_cfc0(13);
  v40 = HIDWORD(UIDcontrolBlockWithType);
  if ( (u32)UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v37);
    return -2147352162;
  }
  v17 = *(u16 *)(v35 + 22) & 2;
  if ( v11 >= 0 )
    v17 = 2;
  if ( v17 == 0 )
  {
    sceKernelCpuResumeIntr(v37);
    return -2147352367;
  }
  v18 = (u32 *)(v35 + 4 * *(u8 *)(TMAN_BSS_U32(0x524) + 21));
  v19 = v18[5];
  v20 = (u32 *)(v35 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
  if ( v19 != 0 && v19 < v8 )
  {
    sceKernelCpuResumeIntr(v37);
    return -2147352132;
  }
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(102, v6, 4, a1);
  v21 = v18[1];
  if ( v21 == 1 && ((*v20 & 0x100) == 0 || v20[1] != 0 || *(u32 *)(TMAN_BSS_U32(0x40) + 16) >= *(u32 *)(v20[3] + 16)) )
    goto LABEL_31;
  if ( a4 == 0 )
  {
    v22 = v18[5] - v18[6];
    if ( v21 == 2 )
    {
      v33 = (u32 *)v20[3];
      if ( v33 == v20 + 3 )
      {
        v23 = v22 < v8;
      }
      else
      {
        do
        {
          v34 = v33[25];
          v33 = (u32 *)*v33;
          v22 += v34;
        }
        while ( v33 != v20 + 3 );
        v23 = v22 < v8;
      }
    }
    else
    {
      v23 = v22 < v8;
    }
    if ( v23 )
      goto LABEL_31;
  }
  v24 = v20[3];
  if ( v21 == 2 && (int)v20[2] > 0 )
  {
    for ( i = (int *)v20[3]; ; i = (int *)v24 )
    {
      v29 = *(u32 *)(v24 + 100);
      if ( v29 >= v8 )
        v29 = v8;
      v24 = *i;
      memcpy(i[24], v7, v29);
      v30 = (u32 *)i[27];
      v31 = i[25] - v29;
      i[24] += v29;
      v8 -= v29;
      v7 += v29;
      i[25] = v31;
      if ( v30 != NULL )
        *v30 = i[26] - v31;
      if ( i[28] == 1 || i[25] == 0 )
      {
        v20[1] = 0;
        v32 = tman_sched_fn_6(i);
        v38 += v32;
      }
      else
      {
        v20[1] = 1;
      }
      if ( v8 == 0 )
        break;
      if ( v18[1] != 2 || (int)v20[2] <= 0 )
        goto LABEL_19;
    }
    v25 = a4 ^ 1;
    goto LABEL_26;
  }
LABEL_19:
  v25 = a4 ^ 1;
  if ( v8 == 0 )
  {
LABEL_26:
    v27 = v36;
    goto LABEL_27;
  }
  v26 = v18[5] - v18[6];
  if ( v26 >= v8 )
    v26 = v8;
  if ( v25 == 0 && v26 != 0 || a4 == 0 && v26 == v8 )
  {
    tman_mem_fn_1(v18, v7, v26);
    v8 -= v26;
    goto LABEL_26;
  }
  v27 = v36;
LABEL_27:
  if ( v27 != NULL )
    *v36 = v39 - v8;
  if ( v25 == 0 && v39 == v8 || a4 == 0 && v8 != 0 )
  {
LABEL_31:
    sceKernelCpuResumeIntr(v37);
    return -2147352141;
  }
  if ( v38 != 0 )
  {
    TMAN_BSS_U32(0x44) = 0;
    if ( v40 == 0 )
      tman_sched_fn_19(0, v37);
  }
  sceKernelCpuResumeIntr(v37);
  return 0;
}


int sceKernelReceiveMsgPipe(int a1, int a2, unsigned int a3, int a4, u32 *a5, int a6)
{
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  unsigned int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v17;
  int UIDcontrolBlockWithType;
  int v19;
  int v20;
  int v21;
  int v22;
  u32 *v23;
  u32 *v24;
  unsigned int v25;
  u32 *v26;
  int v27;
  void (*v28)(int, int, int, int, int, unsigned int, int);
  int v29;
  u32 *v30;
  int v31;
  int v32;
  int v33;
  int v34;
  int v35;
  unsigned int v36;
  int v37;
  unsigned int v38;
  u32 *v39;
  int v40;
  int v41;
  int v42;
  int v43;
  int v44;
  int *v45;
  char v46;
  unsigned int v47;
  int v48;
  unsigned int v49;
  int v50;
  int v51;
  u32 *v52;
  int v53;
  int v54;
  u8 v55[16];
  int v56;
  int v57;
  int v58;
  unsigned int v59;
  int v60;
  int v61;
  int v62;
  int v63;
  unsigned int v64;
  int v65;
  int v66;
  int v67;
  int v68;
  int v69;
  int v70;
  int v71;
  int v72;

  v10 = a2;
  v11 = a3;
  v72 = v9;
  v71 = v8;
  v70 = v6;
  v57 = a1;
  v58 = a4;
  v59 = (unsigned int)a5;
  v60 = a6;
  v64 = a3;
  v12 = (a2 + a3) | a2 | a3;
  v13 = v7 << 11;
  v14 = (v7 << 11) & v12;
  v66 = 0;
  v15 = v7 << 11;
  if ( v14 < 0 || (((v59 + 4) | v59) & v13 & 0x80000000) != 0 || (((v60 + 4) | v60) & v13) < 0 )
    return -2147352365;
  if ( (v58 & 0xFFFFFFFE) != 0 )
    return -2147352171;
  tman_cfc0(13);
  if ( v6 != 0 )
    return -2147352476;
  v61 = sceKernelCpuSuspendIntr(v14, v12);
  if ( v61 == 0 || TMAN_BSS_U32(0x4C) != 0 )
  {
    sceKernelCpuResumeIntr(v61);
    return -2147352153;
  }
  tman_thread_fn_17(v17);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(v57, TMAN_BSS_U32(0x524), &v56);
  v19 = v56;
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v61);
    return -2147352162;
  }
  v20 = *(u16 *)(v56 + 22) & 4;
  if ( v15 >= 0 )
    v20 = 4;
  v21 = 0x20000u;
  if ( v20 == 0 )
  {
    sceKernelCpuResumeIntr(v61);
    return -2147352367;
  }
  v22 = TMAN_BSS_U32(0x524);
  v23 = (u32 *)TMAN_BSS_U32(0x40);
  v24 = (u32 *)(v56 + 4 * *(u8 *)(TMAN_BSS_U32(0x524) + 21));
  v25 = v24[5];
  v26 = (u32 *)(v56 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
  if ( v25 != 0 )
  {
    v19 = v25 < v11;
    if ( v25 < v11 )
    {
      sceKernelCpuResumeIntr(v61);
      return -2147352132;
    }
  }
  v27 = v66;
  if ( v66 == 0 )
  {
    v28 = (void (*)(int, int, int, int, int, unsigned int, int))TMAN_BSS_U32(0x48);
    if ( TMAN_BSS_U32(0x48) == NULL )
    {
      v30 = (u32 *)v60;
      goto LABEL_20;
    }
    v29 = 103;
    goto LABEL_81;
  }
  v28 = (void (*)(int, int, int, int, int, unsigned int, int))TMAN_BSS_U32(0x48);
  v29 = 104;
  if ( TMAN_BSS_U32(0x48) != NULL )
  {
LABEL_81:
    v28(v29, v8, 4, v57, v10, v11, v58);
    v30 = (u32 *)v60;
    goto LABEL_20;
  }
  v30 = (u32 *)v60;
LABEL_20:
  v31 = v58;
  if ( v30 != NULL )
  {
    tman_mem_fn_34(v55, *v30);
    v31 = v58;
  }
  v32 = v66;
  v33 = v31 ^ 1;
  v67 = v31 != 1;
  v69 = v66 != 0;
  v68 = v31 != 0;
  do
  {
    if ( v66 != 0 )
    {
      v35 = v23[39];
      if ( v35 != 0 )
      {
        tman_sched_fn_21(v61, v32, v19, v33, v35, v22, v21, v27);
        if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v57, TMAN_BSS_U32(0x524), &v56) != 0 )
        {
          sceKernelCpuResumeIntr(v61);
          return -2147352139;
        }
        v34 = v24[1];
      }
      else
      {
        v34 = v24[1];
      }
    }
    else
    {
      v34 = v24[1];
    }
    v63 = 0;
    v65 = 0;
    if ( v34 == 2 )
    {
      if ( (*v26 & 0x1000) != 0 )
      {
        if ( v26[1] == 0 && v23[4] < *(u32 *)(v26[3] + 16) )
        {
          v36 = v24[6];
          goto LABEL_29;
        }
        v65 = 1;
      }
      else
      {
        v65 = 1;
      }
LABEL_35:
      v39 = (u32 *)v59;
      goto LABEL_36;
    }
    v36 = v24[6];
LABEL_29:
    if ( v36 != 0 )
    {
      v38 = v36;
      if ( v36 >= v11 )
        v38 = v11;
      tman_mem_fn_2(v24, v10, v38);
      v34 = v24[1];
      v11 -= v38;
      v10 += v38;
      v37 = v26[3];
    }
    else
    {
      v37 = v26[3];
    }
    v62 = v37;
    if ( v34 != 1 )
      goto LABEL_35;
    v39 = (u32 *)v59;
    if ( (int)v26[2] <= 0 )
      goto LABEL_36;
    v45 = (int *)v62;
    do
    {
      v46 = 0;
      v62 = *v45;
      if ( v11 != 0 )
      {
        v47 = v45[25];
        if ( v47 >= v11 )
          v47 = v11;
        memcpy(v10, v45[24], v47);
        v10 += v47;
        v48 = v45[25] - v47;
        v45[24] += v47;
        v11 -= v47;
        v46 = 1;
        v45[25] = v48;
      }
      v49 = v45[25];
      v50 = v45[28];
      if ( v24[5] - v24[6] < v49 )
        v49 = v24[5] - v24[6];
      if ( v50 == 1 && v49 != 0 )
      {
        v51 = v45[24];
LABEL_71:
        tman_mem_fn_1(v24, v51, v49);
        v46 = 1;
        v45[25] -= v49;
        goto LABEL_60;
      }
      if ( v50 != 0 )
      {
        v52 = (u32 *)v45[27];
        goto LABEL_61;
      }
      if ( v49 == v45[25] )
      {
        v51 = v45[24];
        goto LABEL_71;
      }
LABEL_60:
      v52 = (u32 *)v45[27];
LABEL_61:
      if ( v52 != NULL )
        *v52 = v45[26] - v45[25];
      if ( ((v45[28] == 1) & (u8)v46) != 0 )
      {
        v26[1] = 0;
      }
      else
      {
        if ( v45[25] != 0 )
        {
          v26[1] = 1;
          goto LABEL_35;
        }
        v26[1] = 0;
      }
      v53 = tman_sched_fn_6(v45);
      v54 = v24[1];
      v63 += v53;
      if ( v54 != 1 )
        goto LABEL_35;
      v45 = (int *)v62;
    }
    while ( (int)v26[2] > 0 );
    v39 = (u32 *)v59;
LABEL_36:
    if ( v39 != NULL )
      *v39 = v64 - v11;
    v40 = v64 != v11;
    if ( v67 || v40 )
    {
      v41 = 0;
      if ( v68 || v11 == 0 )
        break;
    }
    v24[1] = 2;
    v63 = 0;
    v23[24] = v10;
    v23[25] = v11;
    v23[26] = v64;
    v23[27] = v59;
    v42 = v58;
    v23[29] = 1;
    v23[28] = v42;
    if ( v65 == 0 )
      v26[1] = v40;
    v43 = v56;
    v44 = v60;
    v23[38] = v66;
    v41 = tman_sched_fn_5(v43, 8, (*v26 >> 4) & 0x100, (int)v55, v44);
    v10 = v23[24];
    v11 = v23[25];
  }
  while ( v69 && v41 == 0x800201AB );
  if ( v63 != 0 )
  {
    TMAN_BSS_U32(0x44) = 0;
    tman_sched_fn_19(0, v61);
  }
  sceKernelCpuResumeIntr(v61);
  return v41;
}


int sceKernelTryReceiveMsgPipe(int a1, int a2, unsigned int a3, int a4, unsigned int a5)
{
  int v5;
  int v6;
  int v7;
  unsigned int v8;
  int v9;
  int v10;
  int v11;
  int v13;
  s64 UIDcontrolBlockWithType;
  int v16;
  u32 *v17;
  unsigned int v18;
  u32 *v19;
  int v20;
  int v21;
  unsigned int v22;
  unsigned int v23;
  int v24;
  unsigned int v25;
  int *v26;
  u32 *v27;
  int *v28;
  char v29;
  unsigned int v30;
  int v31;
  unsigned int v32;
  int v33;
  int v34;
  u32 *v35;
  int v36;
  int v37;
  u32 *v38;
  int v39;
  int v40;
  int v41;
  u32 *v42;
  int v43;
  int v44;
  unsigned int v45;
  int v46;

  v7 = a2;
  v8 = a3;
  v9 = v5 << 11;
  v10 = (v5 << 11) & ((a2 + a3) | a2 | a3);
  v11 = v5 << 11;
  v41 = a4;
  v42 = (u32 *)a5;
  v44 = 0;
  v45 = a3;
  if ( v10 < 0 )
    return -2147352365;
  v13 = ((a5 + 4) | a5) & v9;
  if ( v13 < 0 )
    return -2147352365;
  if ( (v41 & 0xFFFFFFFE) != 0 )
    return -2147352171;
  v43 = sceKernelCpuSuspendIntr(v13, v10);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x524), &v40);
  tman_cfc0(13);
  v46 = HIDWORD(UIDcontrolBlockWithType);
  if ( (u32)UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v43);
    return -2147352162;
  }
  v16 = *(u16 *)(v40 + 22) & 4;
  if ( v11 >= 0 )
    v16 = 4;
  if ( v16 == 0 )
  {
    sceKernelCpuResumeIntr(v43);
    return -2147352367;
  }
  v17 = (u32 *)(v40 + 4 * *(u8 *)(TMAN_BSS_U32(0x524) + 21));
  v18 = v17[5];
  v19 = (u32 *)(v40 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
  if ( v18 != 0 && v18 < v8 )
  {
    sceKernelCpuResumeIntr(v43);
    return -2147352132;
  }
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(105, v6, 4, a1);
  v20 = v17[1];
  if ( v20 == 2 )
  {
    if ( (*v19 & 0x1000) == 0 )
      goto LABEL_30;
    if ( v19[1] != 0 )
      goto LABEL_30;
    v21 = v41;
    if ( *(u32 *)(TMAN_BSS_U32(0x40) + 16) >= *(u32 *)(v19[3] + 16) )
      goto LABEL_30;
  }
  else
  {
    v21 = v41;
  }
  if ( v21 == 0 )
  {
    v23 = v17[6];
    v22 = v23;
    if ( v20 == 1 )
    {
      v38 = (u32 *)v19[3];
      if ( v38 == v19 + 3 )
      {
        v24 = v23 < v8;
      }
      else
      {
        do
        {
          v39 = v38[25];
          v38 = (u32 *)*v38;
          v23 += v39;
        }
        while ( v38 != v19 + 3 );
        v24 = v23 < v8;
      }
    }
    else
    {
      v24 = v23 < v8;
    }
    if ( !v24 )
      goto LABEL_20;
LABEL_30:
    sceKernelCpuResumeIntr(v43);
    return -2147352140;
  }
  v22 = v17[6];
LABEL_20:
  if ( v22 != 0 )
  {
    v25 = v22;
    if ( v22 >= v8 )
      v25 = v8;
    tman_mem_fn_2(v17, v7, v25);
    v20 = v17[1];
    v8 -= v25;
    v7 += v25;
  }
  v26 = (int *)v19[3];
  if ( v20 != 1 )
  {
LABEL_25:
    v27 = v42;
    goto LABEL_26;
  }
  v27 = v42;
  if ( (int)v19[2] > 0 )
  {
    v28 = (int *)v19[3];
    while ( 1 )
    {
      v29 = 0;
      v26 = (int *)*v26;
      if ( v8 != 0 )
      {
        v30 = v28[25];
        if ( v30 >= v8 )
          v30 = v8;
        memcpy(v7, v28[24], v30);
        v31 = v28[24];
        v7 += v30;
        v28[25] -= v30;
        v8 -= v30;
        v28[24] = v31 + v30;
        v29 = 1;
      }
      v32 = v28[25];
      v33 = v28[28];
      if ( v17[5] - v17[6] < v32 )
        v32 = v17[5] - v17[6];
      if ( v33 == 1 && v32 != 0 )
        break;
      if ( v33 == 0 )
      {
        if ( v32 == v28[25] )
        {
          v34 = v28[24];
LABEL_60:
          tman_mem_fn_1(v17, v34, v32);
          v29 = 1;
          v28[25] -= v32;
        }
        v35 = (u32 *)v28[27];
        goto LABEL_50;
      }
      v35 = (u32 *)v28[27];
LABEL_50:
      if ( v35 != NULL )
        *v35 = v28[26] - v28[25];
      if ( ((v28[28] == 1) & (u8)v29) != 0 )
      {
        v19[1] = 0;
      }
      else
      {
        if ( v28[25] != 0 )
        {
          v19[1] = 1;
          goto LABEL_25;
        }
        v19[1] = 0;
      }
      v36 = tman_sched_fn_6(v28);
      v37 = v17[1];
      v44 += v36;
      if ( v37 != 1 )
        goto LABEL_25;
      v28 = v26;
      if ( (int)v19[2] <= 0 )
      {
        v27 = v42;
        goto LABEL_26;
      }
    }
    v34 = v28[24];
    goto LABEL_60;
  }
LABEL_26:
  if ( v27 != NULL )
    *v27 = v45 - v8;
  if ( v41 == 1 && v45 == v8 || v41 == 0 && v8 != 0 )
    goto LABEL_30;
  if ( v44 != 0 )
  {
    TMAN_BSS_U32(0x44) = 0;
    if ( v46 == 0 )
      tman_sched_fn_19(0, v43);
  }
  sceKernelCpuResumeIntr(v43);
  return 0;
}


int sceKernelCancelMsgPipe(int a1, unsigned int a2, unsigned int a3)
{
  int v3;
  int v4;
  int v6;
  int v8;
  int v10;
  int UIDcontrolBlockWithType;
  int v13;
  int v14;
  int v15;
  u32 *v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  u32 v22[4];

  v4 = v3 << 11;
  v6 = ((a2 + 4) | a2) & (v3 << 11);
  v8 = v3 << 11;
  if ( v6 < 0 )
    return -2147352365;
  v10 = (a3 + 4) | a3;
  if ( (v10 & v4) < 0 )
    return -2147352365;
  v13 = sceKernelCpuSuspendIntr(v10 & v4, v10);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x524), v22);
  v14 = v22[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v13);
    return -2147352162;
  }
  else
  {
    v15 = *(u16 *)(v22[0] + 22) & 8;
    if ( v8 >= 0 )
      v15 = 8;
    if ( v15 != 0 )
    {
      v16 = (u32 *)(v22[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x524) + 21));
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, unsigned int, int, int, unsigned int, unsigned int))TMAN_BSS_U32(0x48))(
          106,
          a3,
          3,
          a1,
          a2,
          a3);
        v14 = v22[0];
      }
      v17 = v16[1];
      v18 = sceKernelCallUIDObjFunction(v14, -2032580477, v14);
      v19 = 0;
      v20 = 0;
      if ( v17 == 1 )
      {
        v20 = v18;
      }
      else
      {
        v19 = v18;
        if ( v17 != 2 )
          v19 = 0;
      }
      if ( a2 != 0 )
        *(u32 *)a2 = v20;
      if ( a3 != 0 )
        *(u32 *)a3 = v19;
      v21 = v16[2];
      v16[6] = 0;
      v16[3] = v21;
      v16[1] = 0;
      v16[4] = v21;
      sceKernelCpuResumeIntr(v13);
      return 0;
    }
    else
    {
      sceKernelCpuResumeIntr(v13);
      return -2147352367;
    }
  }
}


int sceKernelReferMsgPipeStatus(int a1, unsigned int *a2)
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
  int *v14;
  int v15;
  int v16;
  unsigned int v17;
  u32 v19[4];

  v4 = (unsigned int)(a2 + 14) | (unsigned int)a2;
  v5 = v4 & (v2 << 11);
  v6 = v2 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v9 = sceKernelCpuSuspendIntr(a1, v4);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x524), v19);
  v10 = v19[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352162;
  }
  else
  {
    v11 = *(u16 *)(v19[0] + 22) & 1;
    if ( v6 >= 0 )
      v11 = 1;
    if ( v11 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int *, int, int, unsigned int *))TMAN_BSS_U32(0x48))(107, &TMAN_BSS_U32(0x40), 2, a1, a2);
        v10 = v19[0];
      }
      v12 = (int *)&TMAN_BSS_U8(0x6DC);
      v13 = (u32 *)(v10 + 4 * *(u8 *)(TMAN_BSS_U32(0x524) + 21));
      v14 = (int *)(v10 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
      do
      {
        ++v12;
        v12[13] = 0;
      }
      while ( v12 != &TMAN_BSS_U32(0x714) );
      v15 = *(u32 *)(v10 + 16);
      TMAN_BSS_U32(0x714) = 56;
      if ( v15 != 0 )
        strncpy(&TMAN_BSS_U32(0x718), v15, 31);
      TMAN_BSS_U32(0x738) = *v14;
      v16 = v13[1];
      if ( v16 == 1 )
      {
        TMAN_BSS_U32(0x744) = v14[2];
      }
      else if ( v16 == 2 )
      {
        TMAN_BSS_U32(0x748) = v14[2];
      }
      TMAN_BSS_U32(0x73C) = v13[5];
      v17 = *a2;
      TMAN_BSS_U32(0x740) = v13[5] - v13[6];
      if ( TMAN_BSS_U32(0x714) < v17 )
        v17 = TMAN_BSS_U32(0x714);
      memcpy(a2, &TMAN_BSS_U32(0x714), v17);
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


