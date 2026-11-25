#include "threadman_internal.h"

int tman_mem_fn_1(u32 *a1, int a2, unsigned int a3)
{
  int v3;
  unsigned int v4;
  unsigned int v6;
  unsigned int v7;
  unsigned int v8;
  int v9;
  unsigned int v10;
  unsigned int v11;
  int result;
  unsigned int v13;

  v3 = a2;
  v4 = a3;
  v6 = a1[4];
  v7 = a1[3];
  if ( v7 >= v6 )
  {
    v8 = a1[5] - (v7 - a1[2]);
    if ( a3 < v8 )
      v8 = a3;
    memcpy(v7, a2, v8);
    v9 = a1[2];
    v10 = a1[3] + v8;
    v11 = v9 + a1[5];
    a1[6] += v8;
    v4 -= v8;
    v3 += v8;
    a1[3] = v10;
    if ( v10 >= v11 )
      a1[3] = v9;
  }
  result = memcpy(a1[3], v3, v4);
  v13 = a1[3] + v4;
  a1[6] += v4;
  a1[3] = v13;
  return result;
}


int tman_mem_fn_2(u32 *a1, int a2, unsigned int a3)
{
  int v3;
  unsigned int v4;
  unsigned int v6;
  unsigned int v7;
  int v8;
  unsigned int v9;
  unsigned int v10;
  int result;
  unsigned int v12;

  v3 = a2;
  v4 = a3;
  v6 = a1[4];
  if ( v6 >= a1[3] )
  {
    v7 = a1[5] - (v6 - a1[2]);
    if ( a3 < v7 )
      v7 = a3;
    memcpy(a2, v6, v7);
    v8 = a1[2];
    v9 = a1[4] + v7;
    v10 = v8 + a1[5];
    a1[6] -= v7;
    v4 -= v7;
    v3 += v7;
    a1[4] = v9;
    if ( v9 >= v10 )
      a1[4] = v8;
  }
  result = memcpy(v3, a1[4], v4);
  v12 = a1[4] + v4;
  a1[6] -= v4;
  a1[4] = v12;
  return result;
}


int tman_mem_fn_3(int a1)
{
  u32 *v1;
  int v2;
  u32 *v3;

  v1 = *(u32 **)TMAN_BSS_U32(0x524);
  if ( *(u32 *)TMAN_BSS_U32(0x524) == TMAN_BSS_U32(0x524) )
    return -1;
  v2 = *(u8 *)(TMAN_BSS_U32(0x524) + 21);
  v3 = &v1[v2];
  while ( v3[7] != a1 )
  {
    v1 = (u32 *)*v1;
    v3 = &v1[v2];
    if ( v1 == (u32 *)TMAN_BSS_U32(0x524) )
      return -1;
  }
  return v1[3] ^ TMAN_BSS_U32(0x844);
}


int tman_mem_fn_4()
{
  sceKernelCreateUIDtypeInherit("WaitQ", "MsgPipe", 48, g_uid_msgpipe_funcs, 0, &TMAN_BSS_U32(0x524));
  return 0;
}


int tman_mem_fn_5(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_mem_fn_6(int a1, int a2, int a3, int a4)
{
  int v7;

  v7 = a1 + 4 * *(u8 *)(a2 + 21);
  if ( *(u32 *)(v7 + 20) != 0 )
    sceKernelFreePartitionMemory(*(u32 *)(v7 + 28));
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return 0;
}


int tman_mem_fn_7(int a1)
{
  int v1;
  int v2;
  u32 *v3;
  int v4;
  int *v5;
  int *v7;
  unsigned int v8;
  int v9;
  u32 *v10;
  unsigned int v11;

  v1 = 0;
  v2 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
  v3 = (u32 *)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x524) + 21));
  v4 = 0;
  if ( *(int *)(v2 + 8) > 0 && v3[1] == 1 )
  {
    v5 = *(int **)(v2 + 12);
    v7 = (int *)(v2 + 12);
    v4 = 0;
    if ( v5 != (int *)(v2 + 12) )
    {
      do
      {
        v8 = v5[25];
        v9 = v5[28];
        if ( v3[5] - v3[6] < v8 )
          v8 = v3[5] - v3[6];
        if ( v9 != 1 || v8 == 0 )
        {
          if ( v9 != 0 )
            return v1;
          if ( v8 != v5[25] )
            return v1;
        }
        tman_mem_fn_1(v3, v5[24], v8);
        v10 = (u32 *)v5[27];
        v11 = v5[25] - v8;
        v5[25] = v11;
        if ( v10 != NULL )
          *v10 = v5[26] - v11;
        v1 += tman_sched_fn_6(v5);
      }
      while ( v5 != v7 );
      return v1;
    }
  }
  return v4;
}


int tman_mem_fn_8(int a1)
{
  u32 *v1;
  u32 *v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int result;

  v1 = (u32 *)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
  v2 = (u32 *)v1[3];
  v3 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x524) + 21);
  v1[1] = 0;
  if ( v2 != v1 + 3 )
  {
    v4 = *(u32 *)(v3 + 4);
    if ( v4 == 1 )
    {
      v5 = v2[25];
    }
    else
    {
      if ( v4 != 2 )
        goto LABEL_5;
      v5 = v2[25];
    }
    v1[1] = v2[26] != v5;
  }
LABEL_5:
  v6 = v1[2];
  result = 0;
  v1[2] = v6 - 1;
  if ( v6 == 1 )
    *(u32 *)(v3 + 4) = 0;
  return result;
}


int sceKernelCreateVpl(int a1, int a2, int a3, int a4, int a5)
{
  int v5;

  if ( ((v5 << 11) & a1) < 0 || (((a5 + 4) | a5) & (v5 << 11)) < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a4 != 0 )
    return -2147352476;
  if ( (a3 & 0xFFFFBC00) != 0 )
    return -2147352175;
  return -2147352137;
}


int sceKernelDeleteVpl(int a1, int a2)
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
    if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(0, TMAN_BSS_U32(0x528), v9) != 0 )
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352164;
    }
    else
    {
      v7 = *(u16 *)(v9[0] + 22) & 0x10;
      if ( v5 >= 0 )
        v7 = 16;
      if ( v7 != 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((int (*)())TMAN_BSS_U32(0x48))(109, a1, 1, a1);
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


int sceKernelAllocateVpl(int a1, unsigned int a2, int *a3, int a4, int a5, int a6)
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
  u32 *v21;
  u32 *v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;
  u8 v28[16];
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  int v34;

  v34 = v7;
  v30 = a1;
  v11 = (unsigned int)(a3 + 1) | (unsigned int)a3;
  v12 = v6 << 11;
  v13 = v11 & (v6 << 11);
  v14 = v6 << 11;
  if ( v13 < 0 )
    return -2147352365;
  v15 = ((a4 + 4) | a4) & v12;
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
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v30, TMAN_BSS_U32(0x528), &v29) != 0 )
  {
    sceKernelCpuResumeIntr(v19);
    return -2147352164;
  }
  v20 = *(u16 *)(v29 + 22) & 2;
  if ( v14 >= 0 )
    v20 = 2;
  v21 = (u32 *)TMAN_BSS_U32(0x40);
  if ( v20 == 0 )
  {
    sceKernelCpuResumeIntr(v19);
    return -2147352367;
  }
  v22 = (u32 *)(v29 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
  v23 = v29 + 4 * *(u8 *)(TMAN_BSS_U32(0x528) + 21);
  v31 = v23;
  if ( a2 == 0 || *(u32 *)(v23 + 8) < a2 )
  {
    sceKernelCpuResumeIntr(v19);
    return -2147352137;
  }
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((void (*)(int, int *, int, int, unsigned int))TMAN_BSS_U32(0x48))(110, &TMAN_BSS_U32(0x40), 2, v30, a2);
  if ( a4 != 0 )
    tman_mem_fn_34(v28, *(u32 *)a4);
  v32 = 0;
  v33 = -2147352149;
  while ( 1 )
  {
    v24 = v31;
    if ( (*v22 & 0x200) != 0 || (int)v22[2] <= 0 )
      break;
    if ( (*v22 & 0x100) != 0 )
    {
      if ( v21[4] < *(u32 *)(v22[3] + 16) )
      {
        v24 = v31;
        break;
      }
      v21[25] = a2;
    }
    else
    {
      v21[25] = a2;
    }
LABEL_28:
    v21[38] = 0;
    v27 = tman_sched_fn_5(v29, 6, *v22, (int)v28, a4);
    v26 = v27;
    if ( v27 == 0 )
      *a3 = v21[24];
    if ( ((u8)v32 & (v27 == v33)) == 0 )
      goto LABEL_31;
  }
  v25 = sceKernelAllocHeapMemory(*(u32 *)(v24 + 4), a2);
  v26 = 0;
  if ( v25 == 0 )
  {
    v21[25] = a2;
    goto LABEL_28;
  }
  *a3 = v25;
LABEL_31:
  sceKernelCpuResumeIntr(v19);
  return v26;
}


int sceKernelTryAllocateVpl(int a1, unsigned int a2, unsigned int a3)
{
  int v3;
  int v5;
  int v8;
  int v9;
  int v10;
  int v11;
  int v13;
  int v14;
  u32 v15[4];

  v5 = ((a3 + 4) | a3) & (v3 << 11);
  v8 = v3 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v9 = sceKernelCpuSuspendIntr(a1, a2);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x528), v15) != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352164;
  }
  else
  {
    v10 = *(u16 *)(v15[0] + 22) & 2;
    if ( v8 >= 0 )
      v10 = 2;
    if ( v10 != 0 )
    {
      v11 = v15[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x528) + 21);
      if ( a2 != 0 && *(u32 *)(v11 + 8) >= a2 )
      {
        v13 = sceKernelAllocHeapMemory(*(u32 *)(v11 + 4), a2);
        v14 = v13;
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((void (*)(int, unsigned int, int, int, unsigned int, int))TMAN_BSS_U32(0x48))(112, a2, 3, a1, a2, v13);
        if ( v14 != 0 )
        {
          *(u32 *)a3 = v14;
          sceKernelCpuResumeIntr(v9);
          return 0;
        }
        else
        {
          sceKernelCpuResumeIntr(v9);
          return -2147352176;
        }
      }
      else
      {
        sceKernelCpuResumeIntr(v9);
        return -2147352137;
      }
    }
    else
    {
      sceKernelCpuResumeIntr(v9);
      return -2147352367;
    }
  }
}


int sceKernelFreeVpl(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int *v5;
  int v6;
  int v9;
  int v10;
  int v11;
  u32 *v12;
  int v13;
  int v14;
  int *v15;
  int v16;
  int i;
  int v18;
  int *v19;
  int v20;
  int v22[4];

  v4 = (v2 << 11) & a2;
  v5 = NULL;
  v6 = v2 << 11;
  if ( v4 < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a2 != 0 )
    return -2147352476;
  v9 = sceKernelCpuSuspendIntr(a1, 0);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x528), v22) != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352164;
  }
  v10 = *(u16 *)(v22[0] + 22) & 2;
  if ( v6 >= 0 )
    v10 = 2;
  if ( v10 == 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352367;
  }
  v11 = v22[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x528) + 21);
  v12 = (u32 *)(v22[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
  if ( a2 == 0 )
  {
    v14 = v9;
    goto LABEL_26;
  }
  v13 = sceKernelFreeHeapMemory(*(u32 *)(v11 + 4), a2, v22[0]);
  v14 = v9;
  if ( v13 < 0 )
  {
LABEL_26:
    sceKernelCpuResumeIntr(v14);
    return -2147352138;
  }
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((void (*)(int, int, int, int, int))TMAN_BSS_U32(0x48))(113, v3, 2, a1, a2);
  v15 = (int *)v12[3];
  v16 = 0;
  if ( v15 != v12 + 3 )
  {
    for ( i = *(u32 *)(v11 + 4); ; i = *(u32 *)(v11 + 4) )
    {
      v18 = v15[25];
      v19 = v15;
      v15 = (int *)*v15;
      v20 = sceKernelAllocHeapMemory(i, v18);
      if ( v20 != 0 )
      {
        v19[24] = v20;
        if ( tman_sched_fn_6(v19) > 0 )
        {
          v5 = v19;
          ++v16;
        }
      }
      else if ( (*v12 & 0x200) == 0 )
      {
        break;
      }
      if ( v15 == v12 + 3 )
        break;
    }
  }
  if ( v16 == 1 )
  {
    tman_thread_fn_15(v5, v5[4]);
    tman_thread_fn_16(v5, v9);
  }
  else if ( v16 > 0 )
  {
    TMAN_BSS_U32(0x44) = 0;
    tman_sched_fn_19(0, v9);
  }
  sceKernelCpuResumeIntr(v9);
  return 0;
}


int sceKernelCancelVpl(int a1, int *a2)
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
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x528), v14);
  v10 = v14[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352164;
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
        ((void (*)(int, int *, int, int, int *))TMAN_BSS_U32(0x48))(114, a2, 2, a1, a2);
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


int sceKernelReferVplStatus(int a1, unsigned int *a2)
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
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x528), v18);
  v10 = v18[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352164;
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
        ((void (*)(int, int, int, int, unsigned int *))TMAN_BSS_U32(0x48))(115, a1, 2, a1, a2);
        v10 = v18[0];
      }
      v12 = (int *)&TMAN_BSS_U8(0x6E0);
      v13 = v10 + 4 * *(u8 *)(TMAN_BSS_U32(0x528) + 21);
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
      TMAN_BSS_U32(0x73C) = *(u32 *)(v13 + 8);
      TMAN_BSS_U32(0x740) = sceKernelHeapTotalFreeSize(*(u32 *)(v13 + 4));
      v16 = *a2;
      TMAN_BSS_U32(0x744) = v14[2];
      if ( TMAN_BSS_U32(0x714) < v16 )
        v16 = TMAN_BSS_U32(0x714);
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


int tman_mem_fn_9(int a1)
{
  u32 *v1;
  int v2;
  u32 *v3;

  v1 = *(u32 **)TMAN_BSS_U32(0x528);
  if ( *(u32 *)TMAN_BSS_U32(0x528) == TMAN_BSS_U32(0x528) )
    return -1;
  v2 = *(u8 *)(TMAN_BSS_U32(0x528) + 21);
  v3 = &v1[v2];
  while ( v3[1] != a1 )
  {
    v1 = (u32 *)*v1;
    v3 = &v1[v2];
    if ( v1 == (u32 *)TMAN_BSS_U32(0x528) )
      return -1;
  }
  return v1[3] ^ TMAN_BSS_U32(0x844);
}


int tman_mem_fn_10()
{
  sceKernelCreateUIDtypeInherit("WaitQ", "Vpl", 48, g_uid_vpl_funcs, 0, &TMAN_BSS_U32(0x528));
  return 0;
}


int tman_mem_fn_11(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_mem_fn_12(int a1, int a2, int a3, int a4)
{
  int v8;

  v8 = *(u32 *)(a1 + 4 * *(u8 *)(a2 + 21) + 4);
  if ( v8 > 0 )
    sceKernelDeleteHeap(v8);
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return 0;
}


int tman_mem_fn_13(int a1)
{
  int v1;
  u32 *v2;
  int v3;
  int result;
  int *v5;
  int i;
  int v7;
  int *v8;
  int v9;

  v1 = 0;
  v2 = (u32 *)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
  v3 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x528) + 21);
  result = 0;
  if ( (int)v2[2] > 0 )
  {
    v5 = (int *)v2[3];
    result = 0;
    if ( v5 != v2 + 3 )
    {
      for ( i = *(u32 *)(v3 + 4); ; i = *(u32 *)(v3 + 4) )
      {
        v7 = v5[25];
        v8 = v5;
        v5 = (int *)*v5;
        v9 = sceKernelAllocHeapMemory(i, v7);
        if ( v9 != 0 )
        {
          v8[24] = v9;
          if ( tman_sched_fn_6(v8) > 0 )
            ++v1;
        }
        else
        {
          result = v1;
          if ( (*v2 & 0x200) == 0 )
            return result;
        }
        if ( v5 == v2 + 3 )
          break;
      }
      return v1;
    }
  }
  return result;
}


int sceKernelCreateFpl(int a1, int a2, int a3, int a4, signed int a5, int *a6)
{
  int v6;
  int v7;
  int v9;
  int v11;
  int v14;
  unsigned int v16;
  int v17;
  int v18;
  unsigned int v19;
  int v20;
  int MemoryPartitionInfo;
  int v22;
  int v23;
  int v24;
  unsigned int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  u32 *v30;
  int v31;
  int v32;
  int v33;
  signed int v34;
  int v35;
  signed int v36;
  int v37[3];
  int v38;
  int v39;
  int v40;
  int v41;
  int v42;
  int v43;
  int BlockHeadAddr;
  int v45;

  v7 = v6 << 11;
  v9 = (v6 << 11) & a1;
  v11 = v6 << 11;
  v40 = a2;
  v41 = a4;
  if ( v9 < 0 )
    return -2147352365;
  v14 = (unsigned int)(a6 + 2) | (unsigned int)a6;
  if ( (v14 & v7) < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a4 != 0 )
    return -2147352476;
  if ( (a3 & 0xFFFFBE00) != 0 )
    return -2147352175;
  if ( v41 == 0 || a5 < 1 )
    return -2147352137;
  v16 = 0;
  v42 = sceKernelCpuSuspendIntr(v14 & v7, v14);
  TMAN_BSS_U32(0x714) = 0;
  TMAN_BSS_U32(0x718) = 0;
  if ( a6 != NULL )
  {
    v17 = *a6;
    if ( (unsigned int)*a6 >= 9 )
      v17 = 8;
    memcpy(&TMAN_BSS_U32(0x714), a6, v17);
    v16 = TMAN_BSS_U32(0x718);
  }
  v18 = (a3 & 0x4000) != 0;
  v19 = (v41 + 3) & 0xFFFFFFFC;
  if ( v16 != 0 )
  {
    v20 = v16 - 1;
    if ( (v16 & (v16 - 1)) != 0 )
    {
      sceKernelCpuResumeIntr(v42);
      return -2147352366;
    }
    if ( v16 < 4 )
    {
      v16 = 4;
      v20 = 3;
    }
    v18 = 4;
    v19 = (v41 + v16 - 1) & ~v20;
    if ( (a3 & 0x4000) == 0 )
      v18 = 3;
  }
  if ( v19 == 0 || !is_mul_ok(a5, v19) )
  {
    sceKernelCpuResumeIntr(v42);
    return -2147352137;
  }
  v37[0] = 16;
  MemoryPartitionInfo = sceKernelQueryMemoryPartitionInfo(v40, v37);
  if ( MemoryPartitionInfo != 0 )
    goto LABEL_31;
  if ( v11 < 0 && (v38 & 3) != 3 )
  {
    sceKernelCpuResumeIntr(v42);
    return -2147352367;
  }
  v45 = 0;
  if ( a5 >= 3 )
  {
    v45 = sceKernelAllocHeapMemory(TMAN_BSS_U32(0x820), 4 * a5);
    if ( v45 == 0 )
    {
LABEL_42:
      sceKernelCpuResumeIntr(v42);
      return -2147352176;
    }
    sceKernelMemset32(v45, 0, 4 * a5);
    v22 = v19 * a5;
  }
  else
  {
    v22 = v19 * a5;
  }
  v43 = sceKernelAllocPartitionMemory(v40, a1, v18, v22, v16);
  if ( v43 <= 0 )
  {
    if ( v45 != 0 )
      sceKernelFreeHeapMemory(TMAN_BSS_U32(0x820), v45, v23);
    goto LABEL_42;
  }
  BlockHeadAddr = sceKernelGetBlockHeadAddr(v43);
  MemoryPartitionInfo = sceKernelCreateUID(TMAN_BSS_U32(0x52C), a1, (u8)(a3 | (v11 >> 31)), &v39);
  if ( MemoryPartitionInfo != 0 )
  {
    if ( v45 != 0 )
      sceKernelFreeHeapMemory(TMAN_BSS_U32(0x820), v45, v24);
    sceKernelFreePartitionMemory(v43);
LABEL_31:
    sceKernelCpuResumeIntr(v42);
    return MemoryPartitionInfo;
  }
  v25 = 0x20000u;
  v26 = *(u32 *)(v39 + 12);
  v27 = TMAN_BSS_U32(0x844);
  v28 = BlockHeadAddr;
  v29 = v45;
  v30 = (u32 *)(v39 + 4 * *(u8 *)(TMAN_BSS_U32(0x52C) + 21));
  *(u32 *)(v39 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21)) = a3;
  v31 = v26 ^ v27;
  v30[1] = v28;
  if ( v29 == 0 )
  {
    v30[9] = 0;
    v45 = (int)(v30 + 9);
    v30[10] = 0;
  }
  v32 = v45;
  v33 = v41;
  v30[7] = v19;
  v34 = 0;
  v35 = v43;
  v30[3] = v32;
  v30[5] = v33;
  *v30 = a6;
  v30[8] = v35;
  v30[6] = a5;
  v30[2] = 0;
  if ( a5 > 0 )
  {
    v36 = 0;
    do
    {
      ++v34;
      tman_mem_fn_14(v30, v36, v25);
      v36 = v34;
    }
    while ( v34 < a5 );
  }
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(116, v30, 1, v31);
  sceKernelCpuResumeIntr(v42);
  return v31;
}


int sceKernelDeleteFpl(int a1, int a2)
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
    if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(0, TMAN_BSS_U32(0x52C), v9) != 0 )
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352163;
    }
    else
    {
      v7 = *(u16 *)(v9[0] + 22) & 0x10;
      if ( v5 >= 0 )
        v7 = 16;
      if ( v7 != 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((int (*)())TMAN_BSS_U32(0x48))(117, a1, 1, a1);
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


int sceKernelAllocateFpl(int a1, unsigned int a2, int a3, int a4, int a5, int a6)
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
  int UIDcontrolBlockWithType;
  int v20;
  int v21;
  int v22;
  int v23;
  u32 *v24;
  int v25;
  int v26;
  u8 v27[16];
  int v28;
  int v29;
  u32 *v30;
  int v31;
  int v32;

  v32 = v7;
  v29 = a1;
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
  if ( v16 != 0 && TMAN_BSS_U32(0x4C) == 0 )
  {
    tman_thread_fn_17(v17);
    UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(v29, TMAN_BSS_U32(0x52C), &v28);
    v20 = v28;
    if ( UIDcontrolBlockWithType != 0 )
    {
      sceKernelCpuResumeIntr(v18);
      return -2147352163;
    }
    else
    {
      v21 = *(u16 *)(v28 + 22) & 2;
      if ( v13 >= 0 )
        v21 = 2;
      v22 = TMAN_BSS_U32(0x40);
      if ( v21 != 0 )
      {
        v23 = *(u8 *)(TMAN_BSS_U32(0x52C) + 21);
        v30 = (u32 *)(v28 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
        v24 = (u32 *)(v28 + 4 * v23);
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((int (*)())TMAN_BSS_U32(0x48))(118, &TMAN_BSS_U32(0x40), 1, v29);
        if ( a3 != 0 )
          tman_mem_fn_34(v27, *(u32 *)a3);
        v31 = 0;
        while ( v24[2] == 0 )
        {
          *(u32 *)(v22 + 152) = 0;
          v25 = tman_sched_fn_5(v28, 7, *v30, (int)v27, a3);
          v26 = (u8)v31 & (v25 == 0x800201AB);
          if ( v25 == 0 )
            *(u32 *)a2 = *(u32 *)(v22 + 96);
          if ( v26 == 0 )
            goto LABEL_20;
        }
        v25 = 0;
        *(u32 *)a2 = v24[1] + v24[7] * tman_mem_fn_16(v24, 7, v20, v27);
LABEL_20:
        sceKernelCpuResumeIntr(v18);
        return v25;
      }
      else
      {
        sceKernelCpuResumeIntr(v18);
        return -2147352367;
      }
    }
  }
  else
  {
    sceKernelCpuResumeIntr(v16);
    return -2147352153;
  }
}


int sceKernelTryAllocateFpl(int a1, unsigned int a2)
{
  int v2;
  int v4;
  int v5;
  int v6;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  u32 *v13;
  int v14;
  void (*v15)(int, u32 *, int, int, int);
  u32 v17[4];

  v4 = (a2 + 4) | a2;
  v5 = v4 & (v2 << 11);
  v6 = v2 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v10 = sceKernelCpuSuspendIntr(a1, v4);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x52C), v17) != 0 )
  {
    sceKernelCpuResumeIntr(v10);
    return -2147352163;
  }
  else
  {
    v11 = *(u16 *)(v17[0] + 22) & 2;
    if ( v6 >= 0 )
      v11 = 2;
    if ( v11 != 0 )
    {
      v12 = *(u8 *)(TMAN_BSS_U32(0x52C) + 21);
      v13 = (u32 *)(v17[0] + 4 * v12);
      if ( v13[2] != 0 )
      {
        v14 = tman_mem_fn_16(v17[0] + 4 * v12, v17[0], v8, v9);
        v15 = (void (*)(int, u32 *, int, int, int))TMAN_BSS_U32(0x48);
        *(u32 *)a2 = v13[1] + v13[7] * v14;
        if ( v15 != NULL )
          v15(120, v13, 2, a1, v14);
        sceKernelCpuResumeIntr(v10);
        return 0;
      }
      else
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((void (*)(int, int, int, int, u32))TMAN_BSS_U32(0x48))(120, v17[0] + 4 * v12, 2, a1, 0);
        sceKernelCpuResumeIntr(v10);
        return -2147352176;
      }
    }
    else
    {
      sceKernelCpuResumeIntr(v10);
      return -2147352367;
    }
  }
}


int sceKernelFreeFpl(int a1, unsigned int a2)
{
  int v2;
  int v3;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 *v10;
  unsigned int v11;
  int v12;
  signed int v13;
  int v14;
  unsigned int v15;
  int v17[4];

  v3 = (v2 << 11) & a2;
  v6 = v2 << 11;
  if ( v3 < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a2 != 0 )
    return -2147352476;
  v7 = sceKernelCpuSuspendIntr(a1, 0);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x52C), v17) != 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352163;
  }
  else
  {
    v8 = *(u16 *)(v17[0] + 22) & 2;
    if ( v6 >= 0 )
      v8 = 2;
    if ( v8 != 0 )
    {
      v9 = v17[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
      v10 = (u32 *)(v17[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x52C) + 21));
      if ( a2 == 0 )
        goto LABEL_20;
      v11 = v10[1];
      if ( a2 < v11 )
        goto LABEL_20;
      v12 = v10[7];
      v13 = a2 - v11;
      if ( a2 >= v11 + v12 * v10[6] )
        goto LABEL_20;
      if ( v12 == 0 )
        _break(0, 7u);
      v14 = v13 / v12;
      v15 = v11 + v12 * (v13 / v12);
      if ( a2 == v15 && *(u32 *)(4 * v14 + v10[3]) == 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((int (*)())TMAN_BSS_U32(0x48))(121, a2, 2, a1);
        if ( *(int *)(v9 + 8) <= 0 )
        {
          tman_mem_fn_14(v10, v14, v15);
        }
        else
        {
          *(u32 *)(*(u32 *)(v9 + 12) + 96) = a2;
          tman_sched_fn_8(v17[0], 0, 0, v7);
        }
        sceKernelCpuResumeIntr(v7);
        return 0;
      }
      else
      {
LABEL_20:
        sceKernelCpuResumeIntr(v7);
        return -2147352138;
      }
    }
    else
    {
      sceKernelCpuResumeIntr(v7);
      return -2147352367;
    }
  }
}


int sceKernelCancelFpl(int a1, int *a2)
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
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x52C), v14);
  v10 = v14[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352163;
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
        ((void (*)(int, int *, int, int, int *))TMAN_BSS_U32(0x48))(122, a2, 2, a1, a2);
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


int sceKernelReferFplStatus(int a1, unsigned int *a2)
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
  u32 *v13;
  int *v14;
  int v15;
  unsigned int v16;
  u32 v18[4];

  v4 = (unsigned int)(a2 + 14) | (unsigned int)a2;
  v5 = v4 & (v2 << 11);
  v7 = v2 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v9 = sceKernelCpuSuspendIntr(a1, v4);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x52C), v18);
  v10 = v18[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352163;
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
        ((void (*)(int, int, int, int, unsigned int *))TMAN_BSS_U32(0x48))(123, a1, 2, a1, a2);
        v10 = v18[0];
      }
      v12 = (int *)&TMAN_BSS_U8(0x6DC);
      v13 = (u32 *)(v10 + 4 * *(u8 *)(TMAN_BSS_U32(0x52C) + 21));
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
      v16 = *a2;
      TMAN_BSS_U32(0x73C) = v13[5];
      if ( TMAN_BSS_U32(0x714) < v16 )
        v16 = TMAN_BSS_U32(0x714);
      TMAN_BSS_U32(0x740) = v13[6];
      TMAN_BSS_U32(0x744) = v13[4];
      TMAN_BSS_U32(0x748) = v14[2];
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


int tman_mem_fn_14(u32 *a1, int a2)
{
  u32 *v2;
  int result;
  int *v4;
  int v5;

  v2 = (u32 *)(a1[3] + 4 * a2);
  result = -1;
  if ( *v2 == 0 )
  {
    v4 = (int *)a1[2];
    ++a1[4];
    if ( v4 != NULL )
    {
      v5 = *v4;
      a1[2] = v2;
      *v2 = v5;
      *v4 = (int)v2;
    }
    else
    {
      a1[2] = v2;
      *v2 = v2;
    }
    return 0;
  }
  return result;
}


int tman_mem_fn_15(u32 *a1, int a2)
{
  u32 *v2;
  int result;
  int *v4;
  int v5;

  v2 = (u32 *)(a1[3] + 4 * a2);
  result = -1;
  if ( *v2 == 0 )
  {
    v4 = (int *)a1[2];
    ++a1[4];
    if ( v4 != NULL )
    {
      v5 = *v4;
      a1[2] = v2;
      *v2 = v5;
      *v4 = (int)v2;
    }
    else
    {
      a1[2] = v2;
      *v2 = v2;
    }
    return 0;
  }
  return result;
}


int tman_mem_fn_16(u32 *a1)
{
  u32 **v1;
  int v2;
  u32 **v3;
  u32 *v4;
  int v5;

  v1 = (u32 **)a1[2];
  v2 = -1;
  v3 = v1;
  if ( v1 != NULL )
  {
    v4 = *v1;
    --a1[4];
    if ( v1 == v4 )
      v3 = NULL;
    else
      *v1 = (u32 *)*v4;
    v5 = a1[3];
    a1[2] = v3;
    *v4 = 0;
    return ((int)v4 - v5) >> 2;
  }
  return v2;
}


int tman_mem_fn_17(u32 *a1)
{
  u32 **v1;
  int v2;
  u32 **v3;
  u32 *v4;
  int v5;

  v1 = (u32 **)a1[2];
  v2 = -1;
  v3 = v1;
  if ( v1 != NULL )
  {
    v4 = *v1;
    --a1[4];
    if ( v1 == v4 )
      v3 = NULL;
    else
      *v1 = (u32 *)*v4;
    v5 = a1[3];
    a1[2] = v3;
    *v4 = 0;
    return ((int)v4 - v5) >> 2;
  }
  return v2;
}


int tman_mem_fn_18(int a1)
{
  u32 *v1;
  int v2;
  u32 *v3;

  v1 = *(u32 **)TMAN_BSS_U32(0x52C);
  if ( *(u32 *)TMAN_BSS_U32(0x52C) == TMAN_BSS_U32(0x52C) )
    return -1;
  v2 = *(u8 *)(TMAN_BSS_U32(0x52C) + 21);
  v3 = &v1[v2];
  while ( v3[8] != a1 )
  {
    v1 = (u32 *)*v1;
    v3 = &v1[v2];
    if ( v1 == (u32 *)TMAN_BSS_U32(0x52C) )
      return -1;
  }
  return v1[3] ^ TMAN_BSS_U32(0x844);
}


int tman_mem_fn_19()
{
  sceKernelCreateUIDtypeInherit("WaitQ", "Fpl", 48, g_uid_fpl_funcs, 0, &TMAN_BSS_U32(0x52C));
  return 0;
}


int tman_mem_fn_20(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_mem_fn_21(int a1, int a2, int a3, int a4)
{
  int v8;

  v8 = a1 + 4 * *(u8 *)(a2 + 21);
  if ( *(u32 *)(v8 + 12) != v8 + 36 )
    sceKernelFreeHeapMemory(TMAN_BSS_U32(0x820), *(u32 *)(v8 + 12), 0x20000u);
  sceKernelFreePartitionMemory(*(u32 *)(v8 + 32));
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return 0;
}


int sceKernelCreateTlspl(int a1, int a2, int a3, int *a4, signed int a5, int *a6)
{
  int v6;
  int v7;
  int v9;
  int v10;
  int v13;
  int v15;
  int *v16;
  int i;
  unsigned int v18;
  int v19;
  unsigned int v20;
  int v21;
  int v22;
  int MemoryPartitionInfo;
  int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  u32 *v29;
  int InitialRandomValue;
  int **v31;
  int v32;
  int v33;
  int v34;
  unsigned int v35;
  int *v36;
  signed int v37;
  int v38;
  void (*v39)(int, int, int, unsigned int);
  int v40; // dc
  int v41[3];
  int v42;
  int v43;
  int v44;
  int v45;
  int *v46;
  int v47;
  int BlockHeadAddr;
  int v49;

  v7 = v6 << 11;
  v9 = (v6 << 11) & a1;
  v10 = v6 << 11;
  v44 = a2;
  v45 = a3;
  v46 = a4;
  if ( v9 < 0 )
    return -2147352365;
  v13 = (unsigned int)(a6 + 2) | (unsigned int)a6;
  if ( (v13 & v7) < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a4 != NULL )
    return -2147352476;
  if ( (v45 & 0xFFFFBE00) != 0 )
    return -2147352175;
  if ( v46 == NULL || a5 < 1 )
    return -2147352137;
  v15 = 0;
  v47 = sceKernelCpuSuspendIntr(v13 & v7, v13);
  v16 = &TMAN_BSS_U32(0x6D4);
  for ( i = TMAN_BSS_U32(0x6D4); ; i = *v16 )
  {
    ++v16;
    if ( i == 0 )
      break;
    if ( ++v15 >= 16 )
      break;
  }
  if ( v15 == 16 )
  {
    sceKernelCpuResumeIntr(v47);
    return -2147352111;
  }
  TMAN_BSS_U32(0x714) = 0;
  TMAN_BSS_U32(0x718) = 0;
  v18 = 0;
  if ( a6 != NULL )
  {
    v19 = *a6;
    if ( (unsigned int)*a6 >= 9 )
      v19 = 8;
    memcpy(&TMAN_BSS_U32(0x714), a6, v19);
    v18 = TMAN_BSS_U32(0x718);
  }
  v20 = ((unsigned int)v46 + 3) & 0xFFFFFFFC;
  v21 = (v45 & 0x4000) != 0;
  if ( v18 != 0 )
  {
    v22 = v18 - 1;
    if ( (v18 & (v18 - 1)) != 0 )
    {
      sceKernelCpuResumeIntr(v47);
      return -2147352366;
    }
    if ( v18 < 4 )
    {
      v18 = 4;
      v22 = 3;
    }
    v21 = 4;
    v20 = ((unsigned int)v46 + v18 - 1) & ~v22;
    if ( (v45 & 0x4000) == 0 )
      v21 = 3;
  }
  if ( v20 == 0 || !is_mul_ok(a5, v20) )
  {
    sceKernelCpuResumeIntr(v47);
    return -2147352137;
  }
  v41[0] = 16;
  MemoryPartitionInfo = sceKernelQueryMemoryPartitionInfo(v44, v41);
  if ( MemoryPartitionInfo != 0 )
    goto LABEL_36;
  if ( v10 < 0 && (v42 & 3) != 3 )
  {
    sceKernelCpuResumeIntr(v47);
    return -2147352367;
  }
  v49 = 0;
  if ( a5 >= 3 )
  {
    v49 = sceKernelAllocHeapMemory(TMAN_BSS_U32(0x820), 4 * a5);
    if ( v49 == 0 )
    {
LABEL_47:
      sceKernelCpuResumeIntr(v47);
      return -2147352176;
    }
    sceKernelMemset32(v49, 0, 4 * a5);
    v24 = v20 * a5;
  }
  else
  {
    v24 = v20 * a5;
  }
  v25 = sceKernelAllocPartitionMemory(v44, a1, v21, v24, v18);
  v27 = v25;
  if ( v25 <= 0 )
  {
    if ( v49 != 0 )
      sceKernelFreeHeapMemory(TMAN_BSS_U32(0x820), v49, v26);
    goto LABEL_47;
  }
  BlockHeadAddr = sceKernelGetBlockHeadAddr(v25);
  MemoryPartitionInfo = sceKernelCreateUID(TMAN_BSS_U32(0x530), a1, (u8)(v45 | (v10 >> 31)), &v43);
  if ( MemoryPartitionInfo != 0 )
  {
    if ( v49 != 0 )
      sceKernelFreeHeapMemory(TMAN_BSS_U32(0x820), v49, v28);
    sceKernelFreePartitionMemory(v27);
LABEL_36:
    sceKernelCpuResumeIntr(v47);
    return MemoryPartitionInfo;
  }
  v29 = (u32 *)(v43 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
  v31 = (int **)(v43 + 4 * *(u8 *)(TMAN_BSS_U32(0x530) + 21));
  InitialRandomValue = sceKernelGetInitialRandomValue();
  v32 = BlockHeadAddr;
  v33 = v45;
  v34 = v49;
  v35 = (*(u32 *)(v43 + 12) ^ InitialRandomValue) & 0xFFFFFF87 | (8 * (v15 & 0xF));
  *(u32 *)(v43 + 12) = v35 ^ InitialRandomValue;
  v31[1] = (int *)v32;
  *v29 = v33;
  if ( v34 == 0 )
  {
    v31[9] = NULL;
    v49 = (int)(v31 + 9);
    v31[10] = NULL;
  }
  v36 = v46;
  v37 = 0;
  v31[3] = (int *)v49;
  v31[5] = v36;
  v31[7] = (int *)v20;
  *v31 = a6;
  v31[8] = (int *)v27;
  v31[6] = (int *)a5;
  v31[2] = NULL;
  if ( a5 > 0 )
  {
    v38 = 0;
    do
    {
      ++v37;
      tman_mem_fn_15(v31, v38);
      v38 = v37;
    }
    while ( v37 < a5 );
  }
  v39 = (void (*)(int, int, int, unsigned int))TMAN_BSS_U32(0x48);
  v40 = TMAN_BSS_U32(0x48) != NULL;
  (&TMAN_BSS_U32(0x40))[v15 + 421] = v35;
  if ( v40 )
    v39(124, v15, 1, v35);
  sceKernelCpuResumeIntr(v47);
  return v35;
}


int sceKernelDeleteTlspl(unsigned int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 *v10;
  int v11;
  int v12;
  int v13;
  int v15;
  int v16;
  int v17;
  int v18;
  u32 v19[4];

  tman_cfc0(13);
  v4 = v3 << 11;
  if ( v2 != 0 )
    return -2147352476;
  v6 = (a1 >> 3) & 0xF;
  v7 = sceKernelCpuSuspendIntr(a1, a2);
  if ( (&TMAN_BSS_U32(0x40))[v6 + 421] != a1 || (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x530), v19) != 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352112;
  }
  v8 = *(u16 *)(v19[0] + 22) & 0x10;
  if ( v4 >= 0 )
    v8 = 16;
  if ( v8 == 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352367;
  }
  v9 = *(u8 *)(TMAN_BSS_U32(0x530) + 21);
  v10 = (u32 *)(v19[0] + 4 * v9);
  v11 = v10[4];
  v12 = v10[6];
  if ( v11 < v12 )
  {
    v13 = 0;
    if ( v11 == v12 - 1 )
    {
      v16 = (&TMAN_BSS_U32(0x40))[0];
      v17 = v6 * 4 + *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 220);
      v13 = *(u32 *)(v17 + 64);
      v15 = v7;
      if ( v13 == 0 )
        goto LABEL_14;
      v18 = tman_mem_fn_23(v19[0] + 4 * v9, *(u32 *)(v17 + 64));
      if ( v18 < 0 )
      {
        sceKernelCpuResumeIntr(v7);
        return v18;
      }
      *(u32 *)(v6 * 4 + *(u32 *)(v16 + 220) + 64) = 0;
      sceKernelMemset32(v13, 0, v10[7]);
    }
    if ( v13 != 0 )
      goto LABEL_10;
    v15 = v7;
LABEL_14:
    sceKernelCpuResumeIntr(v15);
    return -2147352110;
  }
LABEL_10:
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(125, TMAN_BSS_U32(0x40), 1, a1);
  (&TMAN_BSS_U32(0x40))[v6 + 421] = 0;
  sceKernelDeleteUID(a1);
  sceKernelCpuResumeIntr(v7);
  return 0;
}


int sceKernelAllocateTlspl(unsigned int a1, int *a2, int a3, int a4, int a5)
{
  int v5;
  int v6;
  int v8;
  int v11;
  int v12;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  u32 *v20;
  int v21;
  u32 *v22;
  u32 *v23;
  int v24;
  int v25;
  u8 v26[16];
  int v27[4];

  v6 = v5 << 11;
  v8 = ((unsigned int)(a2 + 1) | (unsigned int)a2) & (v5 << 11);
  v11 = v5 << 11;
  if ( v8 < 0 )
    return -2147352365;
  v12 = (a3 + 4) | a3;
  if ( (v12 & v6) < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a5 != 0 )
    return -2147352476;
  v14 = sceKernelCpuSuspendIntr(v12 & v6, v12);
  v16 = v14;
  if ( v14 == 0 || (v17 = (a1 >> 3) & 0xF, TMAN_BSS_U32(0x4C) != 0) )
  {
    sceKernelCpuResumeIntr(v14);
    return -2147352153;
  }
  v18 = 4 * v17;
  if ( (&TMAN_BSS_U32(0x40))[v17 + 421] != a1
    || (tman_thread_fn_17(v15), (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x530), v27) != 0) )
  {
    sceKernelCpuResumeIntr(v16);
    return -2147352112;
  }
  v19 = *(u16 *)(v27[0] + 22) & 2;
  if ( v11 >= 0 )
    v19 = 2;
  v20 = (u32 *)(&TMAN_BSS_U32(0x40))[0];
  if ( v19 == 0 )
  {
    sceKernelCpuResumeIntr(v16);
    return -2147352367;
  }
  v21 = *(u32 *)(v18 + *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 220) + 64);
  if ( v21 == 0 )
  {
    v22 = (u32 *)(v27[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
    v23 = (u32 *)(v27[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x530) + 21));
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((int (*)())TMAN_BSS_U32(0x48))(126, a3, 1, a1);
    if ( a3 != 0 )
      tman_mem_fn_34(v26, *(u32 *)a3);
    if ( v23[2] != 0 )
    {
      v24 = 0;
      v25 = v23[1] + v23[7] * tman_mem_fn_17(v23);
      *(u32 *)(v18 + v20[55] + 64) = v25;
    }
    else
    {
      v20[38] = 0;
      v24 = tman_sched_fn_5(v27[0], 14, *v22, (int)v26, a3);
      if ( v24 != 0 )
      {
LABEL_23:
        sceKernelCpuResumeIntr(v16);
        return v24;
      }
      v25 = v20[24];
      *(u32 *)(v18 + v20[55] + 64) = v25;
    }
    *a2 = v25;
    sceKernelMemset32(v25, 0, v23[7]);
    goto LABEL_23;
  }
  if ( (v11 & v21) < 0 )
  {
    sceKernelCpuResumeIntr(v16);
    return -2147352365;
  }
  *a2 = v21;
  sceKernelCpuResumeIntr(v16);
  return 0;
}


int sceKernelFreeTlspl(unsigned int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v6;
  unsigned int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  u32 *v13;
  int v14;
  int v15;
  void (*v16)(int, int, int, unsigned int, int);
  int v18;
  int v19;

  tman_cfc0(13);
  v4 = v3 << 11;
  if ( v2 != 0 )
    return -2147352476;
  v6 = sceKernelCpuSuspendIntr(a1, a2);
  v7 = 4 * ((a1 >> 3) & 0xF);
  v8 = (&TMAN_BSS_U32(0x40))[v7 / 4 + 421];
  v19 = v6;
  if ( v8 != a1 || (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x530), &v18) != 0 )
  {
    sceKernelCpuResumeIntr(v19);
    return -2147352112;
  }
  else
  {
    v9 = *(u16 *)(v18 + 22) & 2;
    if ( v4 >= 0 )
      v9 = 2;
    v10 = (&TMAN_BSS_U32(0x40))[0];
    if ( v9 != 0 )
    {
      v11 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 220);
      v12 = *(u32 *)(v7 + v11 + 64);
      if ( v12 == 0 )
      {
LABEL_14:
        sceKernelCpuResumeIntr(v19);
        return 0;
      }
      if ( (v4 & v12) < 0 )
      {
        sceKernelCpuResumeIntr(v19);
        return -2147352365;
      }
      else
      {
        v13 = (u32 *)(v18 + 4 * *(u8 *)(TMAN_BSS_U32(0x530) + 21));
        v14 = v18 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21);
        v15 = tman_mem_fn_23(v13, *(u32 *)(v7 + v11 + 64));
        if ( v15 >= 0 )
        {
          v16 = (void (*)(int, int, int, unsigned int, int))TMAN_BSS_U32(0x48);
          *(u32 *)(v7 + *(u32 *)(v10 + 220) + 64) = 0;
          if ( v16 != NULL )
            v16(129, v12, 2, a1, v12);
          if ( *(int *)(v14 + 8) <= 0 )
          {
            sceKernelMemset32(v12, 0, v13[7]);
            tman_mem_fn_15(v13, v15);
          }
          else
          {
            *(u32 *)(*(u32 *)(v14 + 12) + 96) = v12;
            tman_sched_fn_8(v18, 0, 0, v19);
          }
          goto LABEL_14;
        }
        sceKernelCpuResumeIntr(v19);
        return v15;
      }
    }
    else
    {
      sceKernelCpuResumeIntr(v19);
      return -2147352367;
    }
  }
}


int tman_mem_fn_22(int a1, int a2)
{
  int v2;
  int v4;
  int v5;
  int i;
  int v7;
  int v8;
  unsigned int v9;
  u32 *v10;
  int v11;
  int v12;
  int v13[4];

  v2 = a1;
  if ( *(u32 *)(a1 + 220) != 0 )
  {
    v4 = 0;
    v5 = sceKernelCpuSuspendIntr(a1, a2);
    for ( i = *(u32 *)(v2 + 220); ; i = *(u32 *)(v2 + 220) )
    {
      v7 = 4 * v4 + i;
      v8 = *(u32 *)(v7 + 64);
      *(u32 *)(v7 + 64) = 0;
      v9 = (&TMAN_BSS_U32(0x40))[v4 + 421];
      if ( v9 != 0
        && ((v9 >> 3) & 0xF) == v4
        && v8 != 0
        && (unsigned int)sceKernelGetUIDcontrolBlockWithType(v9, TMAN_BSS_U32(0x530), v13) == 0
        && (v10 = (u32 *)(v13[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x530) + 21)),
            v11 = v13[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21),
            (v12 = tman_mem_fn_23(v10, v8)) >= 0) )
      {
        if ( *(int *)(v11 + 8) <= 0 )
        {
          sceKernelMemset32(v8, 0, v10[7]);
          tman_mem_fn_15(v10, v12);
        }
        else
        {
          v2 = *(u32 *)(v11 + 12);
          *(u32 *)(v2 + 96) = v8;
          tman_sched_fn_8(v13[0], 0, 0, v5);
        }
        ++v4;
      }
      else
      {
        ++v4;
      }
      if ( v4 >= 16 )
        break;
    }
    sceKernelCpuResumeIntr(v5);
  }
  return 0;
}


int sceKernelReferTlsplStatus(unsigned int a1, unsigned int *a2)
{
  int v2;
  int v4;
  int v5;
  int v6;
  int v8;
  int v9;
  int UIDcontrolBlockWithType;
  int v11;
  int v12;
  int *v13;
  u32 *v14;
  int *v15;
  int v16;
  int v17;
  unsigned int v18;
  u32 v20[4];

  v4 = (unsigned int)(a2 + 15) | (unsigned int)a2;
  v5 = v4 & (v2 << 11);
  v6 = v2 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v8 = (a1 >> 3) & 0xF;
  v9 = sceKernelCpuSuspendIntr(a1, v4);
  if ( (&TMAN_BSS_U32(0x40))[v8 + 421] != a1
    || (UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x530), v20),
        v11 = v20[0],
        UIDcontrolBlockWithType != 0) )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352112;
  }
  else
  {
    v12 = *(u16 *)(v20[0] + 22) & 1;
    if ( v6 >= 0 )
      v12 = 1;
    if ( v12 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int *, int, unsigned int, unsigned int *))TMAN_BSS_U32(0x48))(130, TMAN_BSS_U32(0x40), 2, a1, a2);
        v11 = v20[0];
      }
      v13 = (int *)&TMAN_BSS_U8(0x6D8);
      v14 = (u32 *)(v11 + 4 * *(u8 *)(TMAN_BSS_U32(0x530) + 21));
      v15 = (int *)(v11 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21));
      do
      {
        ++v13;
        v13[14] = 0;
      }
      while ( v13 != &TMAN_BSS_U32(0x714) );
      v16 = *(u32 *)(v11 + 16);
      TMAN_BSS_U32(0x714) = 60;
      if ( v16 != 0 )
        strncpy(&TMAN_BSS_U32(0x718), v16, 31);
      v17 = *v15;
      TMAN_BSS_U32(0x73C) = v8;
      TMAN_BSS_U32(0x738) = v17;
      v18 = *a2;
      TMAN_BSS_U32(0x740) = v14[5];
      if ( TMAN_BSS_U32(0x714) < v18 )
        v18 = TMAN_BSS_U32(0x714);
      TMAN_BSS_U32(0x744) = v14[6];
      TMAN_BSS_U32(0x748) = v14[4];
      TMAN_BSS_U32(0x74C) = v15[2];
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


unsigned int tman_mem_fn_23(u32 *a1, unsigned int a2)
{
  unsigned int v2;
  int v3;
  unsigned int v4;
  unsigned int v5;

  v2 = a1[1];
  v3 = -2147352138;
  if ( a2 >= v2 )
  {
    v4 = a1[7];
    if ( a2 < v2 + v4 * a1[6] )
    {
      v3 = -2147352138;
      v5 = (a2 - v2) / v4;
      if ( a2 == v2 + v4 * v5 )
      {
        v3 = -2147352138;
        if ( *(u32 *)(4 * v5 + a1[3]) == 0 )
          return (a2 - v2) / v4;
      }
    }
  }
  return v3;
}


int tman_mem_fn_24(int a1)
{
  u32 *v1;
  int v2;
  u32 *v3;

  v1 = *(u32 **)TMAN_BSS_U32(0x530);
  if ( *(u32 *)TMAN_BSS_U32(0x530) == TMAN_BSS_U32(0x530) )
    return -1;
  v2 = *(u8 *)(TMAN_BSS_U32(0x530) + 21);
  v3 = &v1[v2];
  while ( v3[8] != a1 )
  {
    v1 = (u32 *)*v1;
    v3 = &v1[v2];
    if ( v1 == (u32 *)TMAN_BSS_U32(0x530) )
      return -1;
  }
  return v1[3] ^ TMAN_BSS_U32(0x844);
}


int tman_mem_fn_25()
{
  sceKernelCreateUIDtypeInherit("WaitQ", "Tlspl", 48, g_uid_tlspl_funcs, 0, &TMAN_BSS_U32(0x530));
  return 0;
}


int tman_mem_fn_26(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_mem_fn_27(int a1, int a2, int a3, int a4)
{
  int v8;

  v8 = a1 + 4 * *(u8 *)(a2 + 21);
  if ( *(u32 *)(v8 + 12) != v8 + 36 )
    sceKernelFreeHeapMemory(TMAN_BSS_U32(0x820), *(u32 *)(v8 + 12), 0x20000u);
  sceKernelFreePartitionMemory(*(u32 *)(v8 + 32));
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return 0;
}


int tman_mem_fn_28()
{
  int v0;
  int v1;
  int v2;
  int v3;
  int v4;
  int v5;
  u32 *v6;
  int v7;
  int v8;
  u32 *v9;
  int i;
  int v12[4];
  u32 v13[4];

  TMAN_BSS_U32(0x834) = 1;
  TMAN_BSS_U32(0x830) = 1;
  sceKernelUSec2SysClock(100, v12);
  TMAN_BSS_U32(0x840) = (int)tman_mem_fn_32;
  v0 = 19;
  TMAN_BSS_U32(0x82C) = 2 * v12[0];
  TMAN_BSS_U32(0x83C) = (int)sceKernelGetSystemTimeLow;
  TMAN_BSS_U32(0x828) = v12[0];
  if ( TMAN_BSS_U32(0x808) != 0 )
    v0 = 4;
  TMAN_BSS_U32(0x80C) = v0;
  (*(volatile u32 *)(0xBC60000CU)) = 1;
  (*(volatile u32 *)(0xBC600008U)) = 48;
  (*(volatile u32 *)(0xBC600010U)) = 0;
  _sync();
  sceKernelRegisterIntrHandler(TMAN_BSS_U32(0x80C), 1, tman_mem_fn_29, TMAN_BSS_U32(0x40), 0);
  sceKernelSetIntrLevel(TMAN_BSS_U32(0x80C), 2);
  TMAN_BSS_U32(0x6C) = (int)&TMAN_BSS_U32(0x68);
  TMAN_BSS_U32(0xAC) = (int)&TMAN_BSS_U32(0xA8);
  v1 = 31;
  TMAN_BSS_U32(0x68) = (int)&TMAN_BSS_U32(0x68);
  TMAN_BSS_U32(0xA8) = (int)&TMAN_BSS_U32(0xA8);
  TMAN_BSS_U32(0x64) = 0;
  TMAN_BSS_U32(0xA4) = 0;
  v4 = sceKernelCpuSuspendIntr(v2, v3);
  sceKernelCreateUID(TMAN_BSS_U32(0x53C), "SceThreadmanSysTimerOVF", 0, v13);
  v5 = TMAN_BSS_U32(0xAC);
  v6 = (u32 *)(v13[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21));
  v6[3] = 0;
  v6[2] = -268435456;
  TMAN_BSS_U32(0xBC) = 0;
  TMAN_BSS_U32(0xB8) = 0;
  v7 = v6[2];
  v6[1] = v5;
  TMAN_BSS_U32(0xC0) = v7;
  TMAN_BSS_U32(0xAC) = (int)v6;
  v8 = v6[3];
  v9 = (u32 *)v6[1];
  *v6 = &TMAN_BSS_U32(0xA8);
  TMAN_BSS_U32(0xC4) = v8;
  *v9 = v6;
  for ( i = TMAN_BSS_U32(0x540); ; i = TMAN_BSS_U32(0x540) )
  {
    sceKernelCreateUID(i, "SceThreadmanDelay", 0, v13);
    --v1;
    tman_mem_fn_33(v13[0]);
    if ( v1 < 0 )
      break;
  }
  (*(volatile u32 *)(0xBC600004U)) = TMAN_BSS_U32(0xC0);
  _sync();
  (*(volatile u32 *)(0xBC600000U)) = 0;
  _sync();
  sceKernelSetPrimarySyscallHandler(24, tman_eret);
  sceKernelRegisterSuspendHandler(24, tman_mem_fn_40, 0);
  sceKernelRegisterResumeHandler(24, tman_mem_fn_41, 0);
  sceKernelEnableIntr(TMAN_BSS_U32(0x80C));
  return sceKernelCpuResumeIntr(v4);
}


int tman_mem_fn_29(int a1, u32 *a2)
{
  u32 *v3;
  int v4;
  s64 v5;
  u32 *v6;
  int i;
  s64 v8;
  s64 v9;
  int v10;
  u32 *v11;
  int v12;
  u32 *v13;
  int v14;
  int v15;
  int v16;
  unsigned int v18;
  int v19;
  unsigned int v20;
  u32 *v21;
  int IsKindOf;
  u32 *v23;
  unsigned int v24;
  u32 *v25;
  unsigned int v26;
  s64 v27;

  v3 = a2 + 26;
  v4 = sceKernelCpuSuspendIntr(a1, a2);
  a2[30] = a2[32];
  a2[31] = a2[33];
  v5 = tman_mem_fn_32();
  v6 = (u32 *)a2[26];
  v27 = v5;
  if ( v6 != v3 )
  {
    for ( i = v6[2]; ; i = v6[2] )
    {
      v8 = ((u64)(v8) & 0xFFFFFFFF00000000ULL) | (u32)(i);
      v8 = ((u64)(v8) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v6[3]) << 32);
      v9 = v8 - v27;
      a2[25] = *v6;
      if ( (int)((u64)(v8 - v27) >> 32) > 0 )
        goto LABEL_13;
      if ( HIDWORD(v9) != 0 )
      {
        v10 = *v6;
      }
      else
      {
        if ( (u32)v9 != 0 )
          goto LABEL_13;
        v10 = *v6;
      }
      *(u32 *)(v10 + 4) = v6[1];
      v11 = (u32 *)v6[1];
      v12 = TMAN_BSS_U32(0x53C);
      v6[1] = v6;
      *v11 = v10;
      *v6 = v6;
      v13 = &v6[-*(u8 *)(v12 + 21)];
      if ( sceKernelIsKindOf(v13, TMAN_BSS_U32(0x540)) != 0 )
      {
        v14 = *(u8 *)(TMAN_BSS_U32(0x540) + 21);
        sceKernelCpuResumeIntr(v4);
        ((void (*)(u32))v13[v14])(v13[v14 + 1]);
        v4 = sceKernelCpuSuspendIntr(v15, v16);
        tman_mem_fn_33(v13);
LABEL_10:
        v6 = (u32 *)a2[25];
        goto LABEL_11;
      }
      if ( sceKernelIsKindOf(v13, TMAN_BSS_U32(0x544)) == 0 )
        break;
      v18 = tman_mem_fn_43(v13, v4);
      v19 = 0;
      if ( v18 != 0 )
      {
        v20 = a2[506];
        if ( v18 >= v20 )
          v20 = v18;
        v8 = ((u64)(v8) & 0xFFFFFFFF00000000ULL) | (u32)(v8 + v20);
        v21 = v6;
        v6[3] = HIDWORD(v8) + ((unsigned int)v8 < v20);
LABEL_20:
        v6[2] = v8;
        goto LABEL_21;
      }
      v6[4] = 1;
      tman_thread_fn_4(v13[3] ^ TMAN_BSS_U32(0x844), 0);
      v6 = (u32 *)a2[25];
LABEL_11:
      if ( v6 == v3 )
        goto LABEL_13;
    }
    IsKindOf = sceKernelIsKindOf(v13, TMAN_BSS_U32(0x548));
    v19 = v4;
    if ( IsKindOf == 0 )
    {
      v8 = ((u64)(v8) & 0xFFFFFFFF00000000ULL) | (u32)(v8 - 0x10000000);
      v6[3] = HIDWORD(v8) + ((unsigned int)v8 < 0xF0000000);
      v21 = v6;
      goto LABEL_20;
    }
    v23 = v13;
    v25 = &v13[*(u8 *)(TMAN_BSS_U32(0x548) + 21)];
    v24 = tman_mem_fn_44(v23, v4);
    v26 = v24;
    if ( v24 == 0 )
    {
      v25[6] = 0;
      goto LABEL_10;
    }
    if ( v24 < a2[506] )
      v26 = a2[506];
    v6[3] = HIDWORD(v8) + __CFADD__((u32)v8, v26);
    v6[2] = v8 + v26;
    v19 = 0;
    v21 = v6;
    *(u64 *)(v25 + 3) += v26;
LABEL_21:
    tman_mem_fn_30(v21, v19);
    v6 = (u32 *)a2[25];
    goto LABEL_11;
  }
LABEL_13:
  tman_mem_fn_31();
  a2[25] = 0;
  sceKernelCpuResumeIntr(v4);
  return -1;
}


unsigned int tman_mem_fn_30(int **a1)
{
  int *v1;
  u32 *v3;
  unsigned int i;
  unsigned int result;
  unsigned int v6;
  int v7;

  v1 = (int *)TMAN_BSS_U32(0xA8);
  if ( (int *)TMAN_BSS_U32(0xA8) != &TMAN_BSS_U32(0xA8) )
  {
    v3 = a1 + 2;
    for ( i = *(u32 *)(TMAN_BSS_U32(0xA8) + 12); ; i = v1[3] )
    {
      result = i;
      v6 = v3[1];
      if ( v6 < i )
        break;
      if ( i == v6 )
      {
        if ( (unsigned int)a1[2] < v1[2] )
        {
          v7 = v1[1];
          goto LABEL_9;
        }
        v1 = (int *)*v1;
      }
      else
      {
        v1 = (int *)*v1;
      }
      if ( v1 == &TMAN_BSS_U32(0xA8) )
        break;
    }
  }
  v7 = v1[1];
LABEL_9:
  *a1 = v1;
  a1[1] = (int *)v7;
  v1[1] = (int)a1;
  *a1[1] = (int)a1;
  return result;
}


int tman_mem_fn_31()
{
  int v0;
  s64 v1;
  s64 i;
  s64 v3;
  s64 v4;
  int v5;
  s64 v6;
  s64 v7;
  s64 v8;

  v0 = TMAN_BSS_U32(0xA8);
  v1 = *(u64 *)(TMAN_BSS_U32(0xA8) + 8);
  for ( i = v1 + (unsigned int)TMAN_BSS_U32(0x82C); (int *)v0 != &TMAN_BSS_U32(0xA8); v1 = v3 )
  {
    v3 = *(u64 *)(v0 + 8);
    v4 = i - v3;
    if ( (int)((u64)(i - v3) >> 32) <= 0 )
    {
      if ( HIDWORD(v4) != 0 || (u32)v4 == 0 )
        break;
      v0 = *(u32 *)v0;
    }
    else
    {
      v0 = *(u32 *)v0;
    }
  }
  v8 = tman_mem_fn_32();
  v5 = (unsigned int)v1 < (unsigned int)v8;
  v6 = v8;
  v7 = ((u64)(v7) & 0xFFFFFFFF00000000ULL) | (u32)(v1 - v8);
  v8 = ((u64)(v8) & 0xFFFFFFFF00000000ULL) | (u32)(HIDWORD(v1) - HIDWORD(v8));
  v7 = ((u64)(v7) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(HIDWORD(v1) - HIDWORD(v8) - v5) << 32);
  if ( v7 < (unsigned int)TMAN_BSS_U32(0x82C) )
    v1 = v6 + (unsigned int)TMAN_BSS_U32(0x82C);
  TMAN_BSS_U64(0xC0) = v1;
  (*(volatile u32 *)(0xBC600004U)) = v1;
  _sync();
  return v8;
}


int sceKernelUSec2SysClock(int a1, unsigned int a2)
{
  int v2;
  int result;

  result = -2147352365;
  if ( (((a2 + 8) | a2) & (v2 << 11) & 0x80000000) == 0 )
  {
    *(u32 *)a2 = a1;
    *(u32 *)(a2 + 4) = 0;
    return 0;
  }
  return result;
}


int tman_mem_fn_32()
{
  return (*(volatile u32 *)(0xBC600000U));
}


int sceKernelCancelAlarm(int a1, int a2)
{
  int v2;
  int v4;
  int UIDcontrolBlockWithType;
  int v6;
  int v7;
  int v8;
  int v9;
  int *v10;
  int v11;
  int *v12;
  int v13;
  int v14;
  u32 v16[4];

  v4 = v2 << 11;
  if ( TMAN_BSS_U32(0xA0) == a1 )
    return -2147352161;
  v7 = sceKernelCpuSuspendIntr(a1, a2);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x544), v16);
  v8 = v16[0];
  if ( UIDcontrolBlockWithType == 0 )
  {
    v9 = *(u16 *)(v16[0] + 22) & 0x10;
    if ( v4 >= 0 )
      v9 = 16;
    if ( v9 == 0 )
    {
      sceKernelCpuResumeIntr(v7);
      return -2147352367;
    }
    if ( TMAN_BSS_U32(0x48) != NULL )
    {
      ((int (*)())TMAN_BSS_U32(0x48))(140, a1, 1, a1);
      v8 = v16[0];
    }
    v10 = (int *)(v8 + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21));
    if ( v10[4] == 0 )
    {
      if ( v10 == (int *)TMAN_BSS_U32(0xA4) )
        TMAN_BSS_U32(0xA4) = *v10;
      v11 = *v10;
      tman_cfc0(13);
      *(u32 *)(v11 + 4) = v10[1];
      v12 = (int *)v10[1];
      v10[1] = (int)v10;
      *v12 = v11;
      *v10 = (int)v10;
      if ( v6 != 0 )
      {
        v10[4] = 1;
        v13 = tman_thread_fn_4(a1, v8);
      }
      else
      {
        v13 = sceKernelDeleteUID(a1);
      }
      v14 = v13;
      if ( v13 == 0 )
        tman_mem_fn_31();
      sceKernelCpuResumeIntr(v7);
      return v14;
    }
  }
  sceKernelCpuResumeIntr(v7);
  return -2147352161;
}


int tman_mem_fn_33(int a1)
{
  int result;
  int v2;
  u32 *v3;
  int v4;
  u32 *v5;

  result = TMAN_BSS_U32(0x53C);
  v2 = TMAN_BSS_U32(0x64) + 1;
  v3 = (u32 *)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21));
  if ( TMAN_BSS_U32(0x64) >= 32 )
  {
    v3[4] = 1;
    return tman_thread_fn_4(*(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844), v3);
  }
  else
  {
    v4 = TMAN_BSS_U32(0x6C);
    *v3 = &TMAN_BSS_U32(0x68);
    v3[1] = v4;
    TMAN_BSS_U32(0x6C) = (int)v3;
    v5 = (u32 *)v3[1];
    TMAN_BSS_U32(0x64) = v2;
    *v5 = v3;
  }
  return result;
}


int sceKernelReferAlarmStatus(int a1, unsigned int *a2)
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
  int v14;
  int v15;
  int v16;
  int v17;
  unsigned int v18;
  u32 v20[4];

  v4 = (unsigned int)(a2 + 5) | (unsigned int)a2;
  v5 = v4 & (v2 << 11);
  v6 = v2 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v9 = sceKernelCpuSuspendIntr(a1, v4);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x544), v20);
  v10 = v20[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352161;
  }
  else
  {
    v11 = *(u16 *)(v20[0] + 22) & 1;
    if ( v6 >= 0 )
      v11 = 1;
    if ( v11 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int *, int, int, unsigned int *))TMAN_BSS_U32(0x48))(141, TMAN_BSS_U32(0x40), 2, a1, a2);
        v10 = v20[0];
      }
      v12 = (int *)&TMAN_BSS_U8(0x700);
      v13 = v10 + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21);
      v14 = v10 + 4 * *(u8 *)(TMAN_BSS_U32(0x544) + 21);
      do
      {
        ++v12;
        v12[4] = 0;
      }
      while ( v12 != &TMAN_BSS_U32(0x714) );
      TMAN_BSS_U32(0x714) = 20;
      v15 = *(u32 *)(v14 + 4);
      TMAN_BSS_U32(0x718) = *(u32 *)(v13 + 8);
      v16 = *(u32 *)(v13 + 12);
      v17 = *(u32 *)(v14 + 8);
      v18 = *a2;
      TMAN_BSS_U32(0x71C) = v16;
      TMAN_BSS_U32(0x724) = v17;
      if ( v18 > 0x14 )
        v18 = 20;
      TMAN_BSS_U32(0x720) = v15;
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


int sceKernelCreateVTimer(int a1, int a2)
{
  int v2;
  int v3;
  int result;
  int v5;
  int v6;
  int v7;
  int v8;
  void (*v9)(int, int, int, int);
  int v10;
  u32 *v11;
  u32 v12[4];

  v3 = v2 << 11;
  if ( ((v2 << 11) & a1) < 0 || (((a2 + 4) | a2) & v3) < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a2 != 0 )
    return -2147352476;
  result = sceKernelCreateUID(TMAN_BSS_U32(0x548), a1, (u8)(v3 >> 31), v12);
  if ( result == 0 )
  {
    v7 = sceKernelCpuSuspendIntr(v5, v6);
    v8 = v12[0];
    v9 = (void (*)(int, int, int, int))TMAN_BSS_U32(0x48);
    v10 = v7;
    v11 = (u32 *)(v12[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21));
    v11[6] = 0;
    *v11 = 0;
    if ( v9 != NULL )
      v9(142, v2, 1, *(u32 *)(v8 + 12) ^ TMAN_BSS_U32(0x844));
    sceKernelCpuResumeIntr(v10);
    return *(u32 *)(v12[0] + 12) ^ TMAN_BSS_U32(0x844);
  }
  return result;
}


int sceKernelDeleteVTimer(int a1, int a2)
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
    if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(0, TMAN_BSS_U32(0x548), v9) != 0 )
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352130;
    }
    else
    {
      v7 = *(u16 *)(v9[0] + 22) & 0x10;
      if ( v5 >= 0 )
        v7 = 16;
      if ( v7 != 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((int (*)())TMAN_BSS_U32(0x48))(143, a1, 1, a1);
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


int sceKernelGetVTimerBase(int a1, unsigned int a2)
{
  int v2;
  int v4;
  int v5;
  int v7;
  int v8;
  int v9;
  u32 *v10;
  u32 v12[4];

  v4 = (a2 + 8) | a2;
  v5 = v4 & (v2 << 11);
  v7 = v2 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v8 = sceKernelCpuSuspendIntr(a1, v4);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x548), v12) != 0 )
  {
    sceKernelCpuResumeIntr(v8);
    return -2147352130;
  }
  else
  {
    v9 = *(u16 *)(v12[0] + 22) & 1;
    if ( v7 >= 0 )
      v9 = 1;
    if ( v9 != 0 )
    {
      v10 = (u32 *)(v12[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21));
      if ( *v10 != 0 )
      {
        *(u32 *)a2 = v10[1];
        *(u32 *)(a2 + 4) = v10[2];
      }
      else
      {
        *(u32 *)(a2 + 4) = 0;
        *(u32 *)a2 = 0;
      }
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((void (*)(int, unsigned int, int, int, u32, u32))TMAN_BSS_U32(0x48))(
          144,
          a2,
          3,
          a1,
          *(u32 *)(a2 + 4),
          *(u32 *)a2);
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


s64 sceKernelGetVTimerBaseWide(int a1, int a2)
{
  int v2;
  int v3;
  int v5;
  s64 v6;
  int v7;
  int v8;
  u32 v10[4];

  v5 = sceKernelCpuSuspendIntr(a1, a2);
  v3 = v2 << 11;
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x548), v10) != 0 )
    goto LABEL_10;
  v6 = 0;
  v7 = *(u16 *)(v10[0] + 22) & 1;
  if ( v3 >= 0 )
    v7 = 1;
  if ( v7 == 0 )
  {
LABEL_10:
    sceKernelCpuResumeIntr(v5);
    return -1;
  }
  else
  {
    v8 = v10[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21);
    if ( *(u32 *)v8 != 0 )
      v6 = *(u64 *)(v8 + 4);
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((void (*)(int, int, int, int, u32))TMAN_BSS_U32(0x48))(151, v5, 2, a1, v6);
    sceKernelCpuResumeIntr(v5);
    return v6;
  }
}


int sceKernelGetVTimerTime(int a1, unsigned int a2)
{
  int v2;
  int v4;
  int v5;
  int v7;
  int v8;
  int v9;
  int v10;
  s64 v12;
  u32 v13[4];

  v4 = (a2 + 8) | a2;
  v5 = v4 & (v2 << 11);
  v7 = v2 << 11;
  if ( v5 < 0 )
    return -2147352365;
  v8 = sceKernelCpuSuspendIntr(a1, v4);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x548), v13) != 0 )
  {
    sceKernelCpuResumeIntr(v8);
    return -2147352130;
  }
  else
  {
    v9 = *(u16 *)(v13[0] + 22) & 1;
    if ( v7 >= 0 )
      v9 = 1;
    if ( v9 != 0 )
    {
      v10 = v13[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21);
      if ( *(u32 *)v10 != 0 )
      {
        v12 = ((u64)(v12) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
        *(u64 *)a2 = v12 - *(u64 *)(v10 + 4);
      }
      else
      {
        *(u32 *)a2 = *(u32 *)(v10 + 4);
        *(u32 *)(a2 + 4) = *(u32 *)(v10 + 8);
      }
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((void (*)(int, int, int, int, u32, u32))TMAN_BSS_U32(0x48))(
          145,
          v10,
          3,
          a1,
          *(u32 *)(a2 + 4),
          *(u32 *)a2);
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


s64 sceKernelGetVTimerTimeWide(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v6;
  int v7;
  int v8;
  int v9;
  s64 v10;
  s64 v12;
  u32 v13[4];

  v3 = v2;
  v4 = v2 << 11;
  v6 = sceKernelCpuSuspendIntr(a1, a2);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x548), v13) != 0 )
    goto LABEL_11;
  v7 = *(u16 *)(v13[0] + 22) & 1;
  if ( v4 >= 0 )
    v7 = 1;
  if ( v7 == 0 )
  {
LABEL_11:
    sceKernelCpuResumeIntr(v6);
    return -1;
  }
  else
  {
    v8 = v13[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21);
    if ( *(u32 *)v8 != 0 )
    {
      v12 = ((u64)(v12) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
      v10 = v12 - *(u64 *)(v8 + 4);
    }
    else
    {
      v9 = *(u32 *)(v8 + 8);
      v10 = ((u64)(v10) & 0xFFFFFFFF00000000ULL) | (u32)(*(u32 *)(v8 + 4));
      v10 = ((u64)(v10) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v9) << 32);
    }
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((void (*)(int, int, int, int, u32))TMAN_BSS_U32(0x48))(152, v3, 2, a1, v10);
    sceKernelCpuResumeIntr(v6);
    return v10;
  }
}


int sceKernelSetVTimerTime(int a1, unsigned int a2)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v8;
  int UIDcontrolBlockWithType;
  int v10;
  int v11;
  int v12;
  int v13;
  int *v14;
  int v15;
  s64 v16;
  int v17;
  s64 v19;
  s64 v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  u32 v27[4];

  tman_cfc0(13);
  v5 = v3 << 11;
  if ( v2 != 0 )
    return -2147352476;
  v8 = (a2 + 8) | a2;
  if ( (v8 & v5) < 0 )
    return -2147352365;
  v10 = sceKernelCpuSuspendIntr(v8 & v5, v8);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x548), v27);
  v11 = v27[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v10);
    return -2147352130;
  }
  else
  {
    v12 = *(u16 *)(v27[0] + 22) & 2;
    if ( v5 >= 0 )
      v12 = 2;
    if ( v12 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int, int, int, u32, u32, u32))TMAN_BSS_U32(0x48))(
          146,
          v4,
          3,
          a1,
          *(u32 *)(a2 + 4),
          *(u32 *)a2,
          v27[0]);
        v11 = v27[0];
      }
      v13 = v11 + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21);
      v14 = (int *)(v11 + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21));
      if ( *(u32 *)v13 != 0 )
      {
        v19 = ((u64)(v19) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
        v16 = v19 - *(u64 *)(v13 + 4);
        v20 = ((u64)(v20) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
        v21 = *(u32 *)(v13 + 24);
        *(u64 *)(v13 + 4) = v20 - *(u64 *)a2;
        if ( v21 != 0 )
        {
          if ( v14 == (int *)TMAN_BSS_U32(0xA4) )
            TMAN_BSS_U32(0xA4) = *v14;
          v22 = *v14;
          v23 = *(u32 *)(v13 + 12);
          v24 = *(u32 *)(v13 + 4);
          v25 = *(u32 *)(v13 + 8);
          v26 = *(u32 *)(v13 + 16);
          *(u32 *)(v22 + 4) = v14[1];
          *(u32 *)v14[1] = v22;
          v14[3] = v26 + v25 + __CFADD__(v23, v24);
          *v14 = (int)v14;
          v14[2] = v23 + v24;
          v14[1] = (int)v14;
          tman_mem_fn_30((int **)v14);
          v17 = HIDWORD(v16);
          tman_mem_fn_31();
        }
        else
        {
          v17 = HIDWORD(v16);
        }
      }
      else
      {
        v16 = ((u64)(v16) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(*(u32 *)(v13 + 4)) << 32);
        v16 = ((u64)(v16) & 0xFFFFFFFF00000000ULL) | (u32)(*(u32 *)(v13 + 8));
        *(u32 *)(v13 + 4) = *(u32 *)a2;
        v15 = v16;
        v16 = ((u64)(v16) & 0xFFFFFFFF00000000ULL) | (u32)(HIDWORD(v16));
        *(u32 *)(v13 + 8) = *(u32 *)(a2 + 4);
        v17 = v15;
      }
      *(u32 *)(a2 + 4) = v17;
      *(u32 *)a2 = v16;
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


s64 sceKernelSetVTimerTimeWide(int a1, int a2, s64 a3)
{
  int v3;
  int v4;
  int v5;
  int v6;
  int UIDcontrolBlockWithType;
  int v10;
  int v11;
  u32 *v12;
  int *v13;
  unsigned int v14;
  unsigned int v15;
  u64 v16;
  s64 v18;
  s64 v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int v27;

  tman_cfc0(13);
  v6 = v4 << 11;
  if ( v3 != 0 )
    return -1;
  v27 = sceKernelCpuSuspendIntr(a1, a2);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x548), &v26);
  v10 = v26;
  if ( UIDcontrolBlockWithType == 0 )
  {
    v11 = *(u16 *)(v26 + 22) & 2;
    if ( v6 >= 0 )
      v11 = 2;
    if ( v11 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int, int, int, u32))TMAN_BSS_U32(0x48))(153, v5, 2, a1, a3);
        v10 = v26;
      }
      v12 = (u32 *)(v10 + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21));
      v13 = (int *)(v10 + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21));
      if ( *v12 != 0 )
      {
        v18 = ((u64)(v18) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
        v16 = v18 - *(u64 *)(v12 + 1);
        v19 = ((u64)(v19) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
        v20 = v12[6];
        *(u64 *)(v12 + 1) = v19 - a3;
        if ( v20 != 0 )
        {
          if ( v13 == (int *)TMAN_BSS_U32(0xA4) )
            TMAN_BSS_U32(0xA4) = *v13;
          v21 = *v13;
          v22 = v12[3];
          v23 = v12[1];
          v24 = v12[4];
          v25 = v12[2];
          *(u32 *)(v21 + 4) = v13[1];
          *(u32 *)v13[1] = v21;
          v13[3] = v24 + v25 + __CFADD__(v22, v23);
          *v13 = (int)v13;
          v13[2] = v22 + v23;
          v13[1] = (int)v13;
          tman_mem_fn_30((int **)v13);
          tman_mem_fn_31();
        }
      }
      else
      {
        v14 = v12[2];
        v15 = v12[1];
        *(u64 *)(v12 + 1) = a3;
        v16 = __PAIR64__(v14, v15);
      }
      sceKernelCpuResumeIntr(v27);
      return v16;
    }
  }
  sceKernelCpuResumeIntr(v27);
  return -1;
}


int sceKernelStartVTimer(int a1, int a2)
{
  int v2;
  int v4;
  int UIDcontrolBlockWithType;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  s64 v13;
  int v14;
  int v15;
  int v16;
  u32 v17[4];

  v4 = v2 << 11;
  if ( TMAN_BSS_U32(0xA0) == a1 )
    return -2147352129;
  v6 = sceKernelCpuSuspendIntr(a1, a2);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x548), v17);
  v7 = v17[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352130;
  }
  else
  {
    v8 = *(u16 *)(v17[0] + 22) & 2;
    if ( v4 >= 0 )
      v8 = 2;
    if ( v8 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((int (*)())TMAN_BSS_U32(0x48))(147, a1, 1, a1);
        v7 = v17[0];
      }
      v9 = 1;
      v10 = v7 + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21);
      v11 = v7 + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21);
      if ( *(u32 *)v10 == 0 )
      {
        *(u32 *)v10 = 1;
        v9 = 0;
        v13 = ((u64)(v13) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
        v14 = *(u32 *)(v10 + 24);
        *(u64 *)(v10 + 4) = v13 - *(u64 *)(v10 + 4);
        if ( v14 != 0 )
        {
          v15 = *(u32 *)(v10 + 12);
          v16 = *(u32 *)(v10 + 4);
          *(u32 *)(v11 + 12) = *(u32 *)(v10 + 16) + *(u32 *)(v10 + 8) + __CFADD__(v15, v16);
          *(u32 *)(v11 + 8) = v15 + v16;
          tman_mem_fn_30((int **)v11);
          tman_mem_fn_31();
        }
      }
      sceKernelCpuResumeIntr(v6);
      return v9;
    }
    else
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352367;
    }
  }
}


int sceKernelStopVTimer(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int UIDcontrolBlockWithType;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int *v12;
  s64 v14;
  int v15;
  int v16;
  int *v17;
  u32 v18[4];

  v4 = v2 << 11;
  if ( TMAN_BSS_U32(0xA0) == a1 )
    return -2147352129;
  v7 = sceKernelCpuSuspendIntr(a1, a2);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x548), v18);
  v8 = v18[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352130;
  }
  else
  {
    v9 = *(u16 *)(v18[0] + 22) & 2;
    if ( v4 >= 0 )
      v9 = 2;
    if ( v9 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((int (*)())TMAN_BSS_U32(0x48))(148, v3, 1, a1);
        v8 = v18[0];
      }
      v10 = 0;
      v11 = v8 + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21);
      v12 = (int *)(v8 + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21));
      if ( *(u32 *)v11 != 0 )
      {
        *(u32 *)v11 = 0;
        v10 = 1;
        v14 = ((u64)(v14) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
        v15 = *(u32 *)(v11 + 24);
        *(u64 *)(v11 + 4) = v14 - *(u64 *)(v11 + 4);
        if ( v15 != 0 )
        {
          if ( v12 == (int *)TMAN_BSS_U32(0xA4) )
            TMAN_BSS_U32(0xA4) = *v12;
          v16 = *v12;
          *(u32 *)(v16 + 4) = v12[1];
          v17 = (int *)v12[1];
          v12[1] = (int)v12;
          *v17 = v16;
          *v12 = (int)v12;
          tman_mem_fn_31();
        }
      }
      sceKernelCpuResumeIntr(v7);
      return v10;
    }
    else
    {
      sceKernelCpuResumeIntr(v7);
      return -2147352367;
    }
  }
}


int sceKernelSetVTimerHandler(int a1, unsigned int a2, int a3, int a4)
{
  int v4;
  int v5;
  int v8;
  int v11;
  int UIDcontrolBlockWithType;
  int v13;
  int v14;
  int v15;
  int *v16;
  int v17;
  int v18;
  int v19;
  int *v20;
  int v21;
  int v22;
  int ModuleGPByAddressForKernel;
  u32 v25[4];

  v8 = v4 << 11;
  if ( TMAN_BSS_U32(0xA0) == a1 )
    return -2147352129;
  v11 = ((a2 + 8) | a2) & v8;
  if ( v11 < 0 || (v8 & a3) < 0 || v8 >= 0 && a3 >= 0 )
    return -2147352365;
  v13 = sceKernelCpuSuspendIntr(a1, v11);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x548), v25);
  v14 = v25[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v13);
    return -2147352130;
  }
  else
  {
    v15 = *(u16 *)(v25[0] + 22) & 2;
    if ( v8 >= 0 )
      v15 = 2;
    if ( v15 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int, int, int, u32, u32, int, int))TMAN_BSS_U32(0x48))(
          149,
          a1,
          5,
          a1,
          *(u32 *)(a2 + 4),
          *(u32 *)a2,
          a3,
          a4);
        v14 = v25[0];
      }
      v16 = (int *)(v14 + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21));
      v17 = *v16;
      v18 = v14 + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21);
      if ( *v16 != 0 && v16[6] != 0 )
      {
        if ( v18 == TMAN_BSS_U32(0xA4) )
          TMAN_BSS_U32(0xA4) = *(u32 *)v18;
        v19 = *(u32 *)v18;
        *(u32 *)(v19 + 4) = *(u32 *)(v18 + 4);
        v20 = *(int **)(v18 + 4);
        *(u32 *)(v18 + 4) = v18;
        *v20 = v19;
        *(u32 *)v18 = v18;
      }
      v16[6] = a3;
      if ( a3 != 0 )
      {
        v21 = a3;
        v16[3] = *(u32 *)a2;
        v22 = *(u32 *)(a2 + 4);
        v16[7] = a4;
        v16[5] = (unsigned int)v8 >> 31;
        v16[4] = v22;
        ModuleGPByAddressForKernel = sceKernelGetModuleGPByAddressForKernel(v21);
        v16[8] = ModuleGPByAddressForKernel;
        if ( ModuleGPByAddressForKernel == 0 )
          v16[8] = v5;
        if ( *v16 == 0 )
          goto LABEL_22;
        *(u64 *)(v18 + 8) = *(u64 *)(v16 + 3) + *(u64 *)(v16 + 1);
        tman_mem_fn_30((int **)v18);
        v17 = *v16;
      }
      if ( v17 != 0 )
        tman_mem_fn_31();
LABEL_22:
      sceKernelCpuResumeIntr(v13);
      return 0;
    }
    sceKernelCpuResumeIntr(v13);
    return -2147352367;
  }
}


int sceKernelSetVTimerHandlerWide(int a1, int a2, int a3, int a4, int a5, int a6)
{
  int v6;
  int v7;
  int v10;
  int UIDcontrolBlockWithType;
  int v15;
  int v16;
  int v17;
  int *v18;
  int v19;
  int v20;
  int v21;
  int *v22;
  int v23; // dc
  u32 v25[4];

  v10 = v6 << 11;
  if ( TMAN_BSS_U32(0xA0) == a1 )
    return -2147352129;
  if ( (v10 & a5) < 0 || v10 >= 0 && a5 >= 0 )
    return -2147352365;
  v15 = sceKernelCpuSuspendIntr(a1, 0x20000u);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x548), v25);
  v16 = v25[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v15);
    return -2147352130;
  }
  else
  {
    v17 = *(u16 *)(v25[0] + 22) & 2;
    if ( v10 >= 0 )
      v17 = 2;
    if ( v17 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int, int, int, int, int, int))TMAN_BSS_U32(0x48))(154, a1, 4, a1, a3, a5, a6);
        v16 = v25[0];
      }
      v18 = (int *)(v16 + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21));
      v19 = *v18;
      v20 = v16 + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21);
      if ( *v18 != 0 && v18[6] != 0 )
      {
        if ( v20 == TMAN_BSS_U32(0xA4) )
          TMAN_BSS_U32(0xA4) = *(u32 *)v20;
        v21 = *(u32 *)v20;
        *(u32 *)(v21 + 4) = *(u32 *)(v20 + 4);
        v22 = *(int **)(v20 + 4);
        *(u32 *)(v20 + 4) = v20;
        *v22 = v21;
        *(u32 *)v20 = v20;
      }
      v18[6] = a5;
      if ( a5 != 0 )
      {
        v18[4] = a4;
        v18[7] = a6;
        v18[3] = a3;
        v18[5] = (unsigned int)v10 >> 31;
        v23 = *v18 != 0;
        v18[8] = v7;
        if ( !v23 )
        {
LABEL_18:
          sceKernelCpuResumeIntr(v15);
          return 0;
        }
        *(u64 *)(v20 + 8) = *(u64 *)(v18 + 3) + *(u64 *)(v18 + 1);
        tman_mem_fn_30((int **)v20);
        v19 = *v18;
      }
      if ( v19 != 0 )
        tman_mem_fn_31();
      goto LABEL_18;
    }
    sceKernelCpuResumeIntr(v15);
    return -2147352367;
  }
}


int sceKernelCancelVTimerHandler(int a1, int a2)
{
  int v2;
  int v4;
  int UIDcontrolBlockWithType;
  int v6;
  int v7;
  int v8;
  u32 *v9;
  int *v10;
  int v11;
  int *v12;
  u32 v14[4];

  v4 = v2 << 11;
  if ( TMAN_BSS_U32(0xA0) == a1 )
    return -2147352129;
  v6 = sceKernelCpuSuspendIntr(a1, a2);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x548), v14);
  v7 = v14[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352130;
  }
  v8 = *(u16 *)(v14[0] + 22) & 2;
  if ( v4 >= 0 )
    v8 = 2;
  if ( v8 == 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352367;
  }
  if ( TMAN_BSS_U32(0x48) != NULL )
  {
    ((int (*)())TMAN_BSS_U32(0x48))(150, a1, 1, a1);
    v7 = v14[0];
  }
  v9 = (u32 *)(v7 + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21));
  v10 = (int *)(v7 + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21));
  if ( *v9 == 0 )
    goto LABEL_14;
  if ( v9[6] != 0 )
  {
    if ( v10 == (int *)TMAN_BSS_U32(0xA4) )
      TMAN_BSS_U32(0xA4) = *v10;
    v11 = *v10;
    *(u32 *)(v11 + 4) = v10[1];
    v12 = (int *)v10[1];
    v10[1] = (int)v10;
    *v12 = v11;
    *v10 = (int)v10;
    tman_mem_fn_31();
LABEL_14:
    v9[6] = 0;
    goto LABEL_15;
  }
  v9[6] = 0;
LABEL_15:
  sceKernelCpuResumeIntr(v6);
  return 0;
}


int sceKernelReferVTimerStatus(int a1, unsigned int *a2)
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
  int *v14;
  int v15;
  int v16;
  int v17;
  int v18;
  unsigned int v19;
  s64 v21;
  u64 v22; // kr00_8
  u32 v23[4];

  v5 = (unsigned int)(a2 + 18) | (unsigned int)a2;
  v6 = v5 & (v2 << 11);
  v7 = v2 << 11;
  if ( v6 < 0 )
    return -2147352365;
  v10 = sceKernelCpuSuspendIntr(a1, v5);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x548), v23);
  v11 = v23[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v10);
    return -2147352130;
  }
  else
  {
    v12 = *(u16 *)(v23[0] + 22) & 1;
    if ( v7 >= 0 )
      v12 = 1;
    if ( v12 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int, int, int, unsigned int *))TMAN_BSS_U32(0x48))(155, v3, 2, a1, a2);
        v11 = v23[0];
      }
      v13 = (int *)&TMAN_BSS_U8(0x6CC);
      v14 = (int *)(v11 + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21));
      do
      {
        ++v13;
        v13[17] = 0;
      }
      while ( v13 != &TMAN_BSS_U32(0x714) );
      v15 = *(u32 *)(v11 + 16);
      TMAN_BSS_U32(0x714) = 72;
      if ( v15 != 0 )
        strncpy(&TMAN_BSS_U32(0x718), v15, 31);
      TMAN_BSS_U32(0x738) = *v14;
      if ( *v14 != 0 )
      {
        TMAN_BSS_U32(0x73C) = v14[1];
        TMAN_BSS_U32(0x740) = v14[2];
        v21 = ((u64)(v21) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
        v22 = v21 - *(u64 *)(v14 + 1);
        TMAN_BSS_U8(0x748) = HIDWORD(v22);
        TMAN_BSS_U8(0x744) = v22;
      }
      else
      {
        TMAN_BSS_U32(0x740) = 0;
        TMAN_BSS_U32(0x73C) = 0;
        TMAN_BSS_U8(0x744) = v14[1];
        TMAN_BSS_U8(0x748) = v14[2];
      }
      v16 = v14[7];
      v17 = v14[6];
      TMAN_BSS_U32(0x74C) = v14[3];
      v18 = v14[4];
      v19 = *a2;
      TMAN_BSS_U32(0x758) = v16;
      TMAN_BSS_U32(0x750) = v18;
      if ( TMAN_BSS_U32(0x714) < v19 )
        v19 = TMAN_BSS_U32(0x714);
      TMAN_BSS_U32(0x754) = v17;
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


int sceKernelReturnFromTimerHandler()
{
  return 0;
}


s64 sceKernelUSec2SysClockWide(unsigned int a1)
{
  return a1;
}


int sceKernelSysClock2USec(unsigned int a1, unsigned int a2, unsigned int a3)
{
  int v3;
  int v5;
  int v6;
  u32 v8[4];

  v5 = ((a2 + 4) | a2) & (v3 << 11);
  v6 = -2147352365;
  if ( (((a1 + 8) | a1) & (v3 << 11) & 0x80000000) == 0 && v5 >= 0 && (((a3 + 4) | a3) & (v3 << 11) & 0x80000000) == 0 )
  {
    if ( a2 != 0 )
    {
      tman_mem_fn_42(v8, a1, 1000000, a3, a3);
      *(u32 *)a2 = v8[0];
    }
    else
    {
      *(u32 *)a3 = *(u32 *)a1;
    }
    return 0;
  }
  return v6;
}


int sceKernelSysClock2USecWide(int a1, int a2, unsigned int a3, unsigned int a4)
{
  int v4;
  int v6;
  u32 v8[4];

  v6 = -2147352365;
  if ( (((a3 + 4) | a3) & (v4 << 11) & 0x80000000) == 0 && (((a4 + 4) | a4) & (v4 << 11) & 0x80000000) == 0 )
  {
    if ( a3 != 0 )
    {
      v8[1] = a2;
      v8[0] = a1;
      tman_mem_fn_42(v8, v8, 1000000, a4, a1);
      *(u32 *)a3 = v8[0];
    }
    else
    {
      *(u32 *)a4 = a1;
    }
    return 0;
  }
  return v6;
}


int sceKernelGetSystemTime(int *a1)
{
  int v1;
  int v3;
  int v4;
  int v5;
  int v6;
  void (*v7)(int, int *, int, int, u32);
  int v8;

  v3 = ((unsigned int)(a1 + 2) | (unsigned int)a1) & (v1 << 11);
  v4 = -2147352365;
  if ( v3 >= 0 )
  {
    v6 = sceKernelCpuSuspendIntr(v3, -2147352576);
    v5 = tman_mem_fn_32();
    v7 = (void (*)(int, int *, int, int, u32))TMAN_BSS_U32(0x48);
    a1[1] = v8;
    *a1 = v5;
    if ( v7 != NULL )
      v7(131, a1, 2, a1[1], *a1);
    sceKernelCpuResumeIntr(v6);
    return 0;
  }
  return v4;
}


s64 sceKernelGetSystemTimeWide(int a1, int a2)
{
  int v2;
  s64 v3;
  int v4;
  s64 v5;

  v4 = sceKernelCpuSuspendIntr(a1, a2);
  v3 = ((u64)(v3) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
  v5 = v3;
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(132, v2, 1, v3);
  sceKernelCpuResumeIntr(v4);
  return v5;
}


int sceKernelGetSystemTimeLow()
{
  return (*(volatile u32 *)(0xBC600000U));
}


int tman_mem_fn_34(u64 *a1, unsigned int a2)
{
  s64 v4;

  v4 = ((u64)(v4) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
  *a1 = v4 + a2;
  return v4;
}


int tman_mem_fn_35(u64 *a1, u64 *a2)
{
  s64 v3;

  v3 = ((u64)(v3) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
  *a1 = v3 + *a2;
  return v3;
}


int sceKernelSetAlarm(int a1, int a2, int a3, int a4, int a5, int a6)
{
  int v6;
  int v8;
  int v11;

  v8 = -2147352365;
  if ( a2 != 0 && ((v6 << 11) & a2) >= 0 )
  {
    v11 = tman_mem_fn_45(a1, 0, a2, a3, a5, a6, -2147352365);
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((int (*)())TMAN_BSS_U32(0x48))(138, a2, 4, a1);
    return v11;
  }
  return v8;
}


int sceKernelSetSysClockAlarm(unsigned int a1, int a2, int a3)
{
  int v3;
  int v6;
  int v8;

  v6 = -2147352365;
  if ( a2 != 0 && (((a1 + 8) | a1) & (v3 << 11) & 0x80000000) == 0 && ((v3 << 11) & a2) >= 0 )
  {
    v8 = tman_mem_fn_45(*(u32 *)a1, *(u32 *)(a1 + 4), a2, a3, *(u32 *)(a1 + 4), 0, 0);
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((int (*)())TMAN_BSS_U32(0x48))(139, a2, 5, *(u32 *)(a1 + 4));
    return v8;
  }
  return v6;
}


int tman_mem_fn_36(s64 *a1, int a2, int a3)
{
  s64 v6;
  s64 v7;
  int result;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  u32 *v15;
  int v16;
  int v17[4];

  v7 = *a1;
  v6 = ((u64)(v6) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
  v6 = ((u64)(v6) & 0x00000000FFFFFFFFULL) | ((u64)(u32)((u64)(v7 - v6) >> 32) << 32);
  result = -2147352152;
  if ( v6 >= 0 )
  {
    if ( TMAN_BSS_U32(0x64) <= 0 )
    {
      if ( sceKernelCreateUID(TMAN_BSS_U32(0x540), "SceThreadmanDelay", 0, v17) != 0 )
        return -2147352176;
      v12 = v17[0];
    }
    else
    {
      v9 = TMAN_BSS_U32(0x68);
      --TMAN_BSS_U32(0x64);
      v10 = *(u32 *)TMAN_BSS_U32(0x68);
      v11 = TMAN_BSS_U32(0x53C);
      *(u32 *)(v10 + 4) = *(u32 *)(TMAN_BSS_U32(0x68) + 4);
      **(u32 **)(v9 + 4) = v10;
      v17[0] = v9 - 4 * *(u8 *)(v11 + 21);
      v12 = v17[0];
    }
    if ( v12 != 0 )
    {
      v13 = *(u8 *)(TMAN_BSS_U32(0x540) + 21);
      v14 = v12 + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21);
      *(u32 *)(v14 + 8) = *(u32 *)a1;
      v15 = (u32 *)(v12 + 4 * v13);
      v16 = *((u32 *)a1 + 1);
      v15[1] = a3;
      *v15 = a2;
      *(u32 *)(v14 + 12) = v16;
      tman_mem_fn_30((int **)v14);
      tman_mem_fn_31();
      return *(u32 *)(v12 + 12) ^ TMAN_BSS_U32(0x844);
    }
    return -2147352176;
  }
  return result;
}


int tman_mem_fn_37(int a1, unsigned int a2)
{
  int UIDcontrolBlockWithType;
  int v4;
  int v5;
  int v6;
  s64 v7;
  s64 v8; // kr00_8
  int v9;
  int v10;
  int *v11;
  u32 v13[4];
  int v14[4];

  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x540), v14);
  v4 = -1;
  if ( UIDcontrolBlockWithType == 0 )
  {
    v5 = v14[0];
    v6 = v14[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21);
    if ( a2 != 0 )
    {
      v7 = ((u64)(v7) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
      v8 = *(u64 *)(v6 + 8) - v7;
      v9 = ~(SHIDWORD(v8) >> 31) >> 31;
      v13[1] = HIDWORD(v8) & v9;
      v13[0] = v8 & v9;
      sceKernelSysClock2USec((unsigned int)v13, 0, a2);
      v5 = v14[0];
    }
    if ( v6 == TMAN_BSS_U32(0xA4) )
      TMAN_BSS_U32(0xA4) = *(u32 *)v6;
    v10 = *(u32 *)v6;
    *(u32 *)(v10 + 4) = *(u32 *)(v6 + 4);
    v11 = *(int **)(v6 + 4);
    *(u32 *)(v6 + 4) = v6;
    *v11 = v10;
    *(u32 *)v6 = v6;
    tman_mem_fn_33(v5);
    tman_mem_fn_31();
    return 0;
  }
  return v4;
}


int tman_mem_fn_38()
{
  sceKernelCreateUIDtype("Timer", 20, g_uid_timer_funcs, 0, &TMAN_BSS_U32(0x53C));
  sceKernelCreateUIDtypeInherit("Timer", "Delay", 48, g_uid_delay_funcs, 0, &TMAN_BSS_U32(0x540));
  sceKernelCreateUIDtypeInherit("Timer", "Alarm", 48, g_uid_alarm_funcs, 0, &TMAN_BSS_U32(0x544));
  sceKernelCreateUIDtypeInherit("Timer", "VTimer", 48, g_uid_vtimer_funcs, 0, &TMAN_BSS_U32(0x548));
  return 0;
}


void nullsub_55()
{
  ;
}


int tman_mem_fn_40()
{
  s64 v0;
  int result;

  TMAN_BSS_U32(0x0) = (*(volatile u32 *)(0xBC600000U));
  sceKernelDisableIntr(TMAN_BSS_U32(0x80C));
  v0 = ((u64)(v0) & 0x00000000FFFFFFFFULL) | ((u64)(u32)((unsigned int)(TMAN_BSS_U64(0xC0) - TMAN_BSS_U32(0x0)) > 0xF0000000) << 32);
  v0 = ((u64)(v0) & 0xFFFFFFFF00000000ULL) | (u32)(TMAN_BSS_U64(0xC0) == TMAN_BSS_U32(0x0));
  result = 0;
  if ( v0 != 0 )
    TMAN_BSS_U32(0x0) = TMAN_BSS_U64(0xC0) - 1;
  return result;
}


int tman_mem_fn_41()
{
  (*(volatile u32 *)(0xBC60000CU)) = 1;
  (*(volatile u32 *)(0xBC600008U)) = 48;
  (*(volatile u32 *)(0xBC600010U)) = 0;
  _sync();
  (*(volatile u32 *)(0xBC600000U)) = TMAN_BSS_U32(0x0);
  _sync();
  (*(volatile u32 *)(0xBC600004U)) = TMAN_BSS_U64(0xC0);
  _sync();
  _sync();
  sceKernelEnableIntr(TMAN_BSS_U32(0x80C));
  return 0;
}


int tman_mem_fn_42(unsigned int *a1, unsigned int *a2, unsigned int a3, unsigned int *a4)
{
  unsigned int v4;
  unsigned int v5;
  int v6;
  int v7;
  unsigned int v8;
  int i;
  unsigned int v10;
  int result;

  v4 = *a2;
  v5 = a2[1];
  v6 = 0;
  v7 = v5 % a3;
  v8 = v5 / a3;
  for ( i = 3; i >= 0; --i )
  {
    v10 = (v7 << 8) | HIBYTE(v4);
    v4 <<= 8;
    result = v6 << 8;
    v6 = (v6 << 8) | HIBYTE(v8);
    v7 = v10 % a3;
    v8 = (v8 << 8) + v10 / a3;
  }
  if ( a1 != NULL )
  {
    *a1 = v8;
    a1[1] = v6;
  }
  if ( a4 != NULL )
    *a4 = v10 % a3;
  return result;
}


int tman_mem_fn_43(int a1, int a2)
{
  u32 *v3;
  int v4;
  int v5;
  int v6;
  int UserIntrStack;

  v3 = (u32 *)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x544) + 21));
  TMAN_BSS_U32(0xA0) = *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(5, v3, 3, *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844));
  if ( *v3 != 0 )
  {
    UserIntrStack = sceKernelGetUserIntrStack();
    v6 = sceKernelCallUserIntrHandler(v3[2], 0, 0, 0, v3[1], UserIntrStack);
  }
  else
  {
    sceKernelCpuResumeIntr(a2);
    v6 = ((int (*)(u32))v3[1])(v3[2]);
    sceKernelCpuSuspendIntr(v4, v5);
  }
  TMAN_BSS_U32(0xA0) = 0;
  return v6;
}


int tman_mem_fn_44(int a1, int a2)
{
  int v3;
  int v4;
  s64 v5;
  int v7;
  int v8;
  int v9;
  int UserIntrStack;
  int v12;
  int v13;
  int v14;
  int v15;
  s64 v16;

  v3 = *(u8 *)(TMAN_BSS_U32(0x548) + 21);
  TMAN_BSS_U32(0xA0) = *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
  v4 = a1 + 4 * v3;
  v5 = ((u64)(v5) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
  v16 = v5 - *(u64 *)(v4 + 4);
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((void (*)(int, int, int, int, u32, u32, u32, u32))TMAN_BSS_U32(0x48))(
      6,
      v4,
      5,
      *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844),
      *(u32 *)(v4 + 12),
      v16,
      *(u32 *)(v4 + 28),
      *(u32 *)(v4 + 24));
  if ( *(u32 *)(v4 + 20) != 0 )
  {
    UserIntrStack = sceKernelGetUserIntrStack();
    v12 = UserIntrStack - 16;
    v13 = TMAN_BSS_U32(0x844);
    *(u32 *)v12 = *(u32 *)(v4 + 12);
    v14 = *(u32 *)(v4 + 24);
    v15 = *(u32 *)(v4 + 28);
    *(u32 *)(v12 + 4) = *(u32 *)(v4 + 16);
    *(u64 *)(v12 + 8) = v16;
    v9 = sceKernelCallUserIntrHandler(
           *(u32 *)(a1 + 12) ^ v13,
           UserIntrStack - 16,
           UserIntrStack - 8,
           v15,
           v14,
           UserIntrStack - 16);
  }
  else
  {
    sceKernelCpuResumeIntr(a2);
    v9 = (*(int (**)(int, int, s64 *, u32))(v4 + 24))(
           *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844),
           v4 + 12,
           &v16,
           *(u32 *)(v4 + 28));
    sceKernelCpuSuspendIntr(v7, v8);
  }
  TMAN_BSS_U32(0xA0) = 0;
  return v9;
}


int tman_mem_fn_45(s64 a1, int a2, unsigned int a3)
{
  int v3;
  unsigned int v4;
  int result;
  int v9;
  int UID;
  int v11;
  unsigned int *v12;
  s64 v13;
  int v14;
  int ModuleGPByAddressForKernel;
  int v16[4];

  if ( v3 < 0 || (result = -2147352365, a2 < 0) )
  {
    v9 = sceKernelCpuSuspendIntr(a1, HIDWORD(a1));
    UID = sceKernelCreateUID(TMAN_BSS_U32(0x544), "SceThreadmanAlarm", (u8)(v3 >> 31), v16);
    v11 = v9;
    if ( UID == 0 )
    {
      v12 = (unsigned int *)(v16[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x544) + 21));
      v14 = v16[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21);
      v13 = ((u64)(v13) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
      *(u64 *)(v14 + 8) = a1 + v13;
      v12[2] = a3;
      *v12 = (unsigned int)v3 >> 31;
      v12[1] = a2;
      ModuleGPByAddressForKernel = sceKernelGetModuleGPByAddressForKernel(a2);
      v12[3] = ModuleGPByAddressForKernel;
      if ( ModuleGPByAddressForKernel == 0 )
        v12[3] = v4;
      tman_mem_fn_30((int **)v14);
      tman_mem_fn_31();
      v11 = v9;
      UID = *(u32 *)(v16[0] + 12) ^ TMAN_BSS_U32(0x844);
    }
    sceKernelCpuResumeIntr(v11);
    return UID;
  }
  return result;
}


int tman_mem_fn_46(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_mem_fn_47(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return 0;
}


int tman_mem_fn_48(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_mem_fn_49(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return 0;
}


int tman_mem_fn_50(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return *(u32 *)(a1 + 12) ^ TMAN_BSS_U32(0x844);
}


int tman_mem_fn_51(int a1, int a2, int a3, int a4)
{
  int v7;
  u32 *v8;
  int *v9;
  int v10;
  int *v11;

  v7 = a1;
  v8 = (u32 *)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x548) + 21));
  v9 = (int *)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x53C) + 21));
  if ( *v8 == 0 )
  {
LABEL_6:
    a1 = v7;
    goto LABEL_7;
  }
  if ( v8[6] != 0 )
  {
    if ( v9 == (int *)TMAN_BSS_U32(0xA4) )
      TMAN_BSS_U32(0xA4) = *v9;
    v10 = *v9;
    *(u32 *)(v10 + 4) = v9[1];
    v11 = (int *)v9[1];
    v9[1] = (int)v9;
    *v11 = v10;
    *v9 = (int)v9;
    tman_mem_fn_31();
    goto LABEL_6;
  }
LABEL_7:
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return 0;
}


int tman_mem_fn_52(int a1, int a2, int a3, int a4)
{
  int v6;
  u32 *v7;
  int result;

  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  v6 = TMAN_BSS_U32(0x844);
  v7 = (u32 *)(a1 + 4 * *(u8 *)(a2 + 21));
  v7[4] = 0;
  result = *(u32 *)(a1 + 12) ^ v6;
  *v7 = v7;
  v7[1] = v7;
  return result;
}


int tman_mem_fn_53(int a1, int a2, int a3, int a4)
{
  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  return 0;
}


