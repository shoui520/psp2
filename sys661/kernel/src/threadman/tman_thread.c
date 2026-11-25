#include "threadman_internal.h"

u8 g_tman_bss[0x860] __attribute__((aligned(8)));

const TmanUidFunc g_uid_waitq_funcs[6] = {
    { 0x5BD49380U, (void *)tman_sched_fn_18 },
    { 0x85FE2B4CU, (void *)tman_sched_fn_17 },
    { 0x86D94883U, (void *)tman_sched_fn_16 },
    { 0xD310D2D9U, (void *)tman_sched_fn_14 },
    { 0x87089863U, (void *)tman_sched_fn_15 },
    { 0, NULL },
};

const TmanUidFunc g_uid_threv_funcs[3] = {
    { 0xD310D2D9U, (void *)tman_sched_fn_29 },
    { 0x87089863U, (void *)tman_sched_fn_30 },
    { 0, NULL },
};

const TmanUidFunc g_uid_cb_funcs[3] = {
    { 0xD310D2D9U, (void *)tman_sched_fn_31 },
    { 0x87089863U, (void *)tman_sched_fn_32 },
    { 0, NULL },
};

const TmanUidFunc g_uid_sema_funcs[4] = {
    { 0x85FE2B4CU, (void *)tman_sync_fn_5 },
    { 0xD310D2D9U, (void *)tman_sync_fn_3 },
    { 0x87089863U, (void *)tman_sync_fn_4 },
    { 0, NULL },
};

const TmanUidFunc g_uid_evf_funcs[4] = {
    { 0x5BD49380U, (void *)tman_sync_fn_8 },
    { 0xD310D2D9U, (void *)tman_sync_fn_9 },
    { 0x87089863U, (void *)tman_sync_fn_10 },
    { 0, NULL },
};

const TmanUidFunc g_uid_mutex_funcs[4] = {
    { 0x85FE2B4CU, (void *)tman_sync_fn_17 },
    { 0xD310D2D9U, (void *)tman_sync_fn_15 },
    { 0x87089863U, (void *)tman_sync_fn_16 },
    { 0, NULL },
};

const TmanUidFunc g_uid_lwmutex_funcs[5] = {
    { 0x5BD49380U, (void *)tman_sync_fn_21 },
    { 0x85FE2B4CU, (void *)tman_sync_fn_22 },
    { 0xD310D2D9U, (void *)tman_sync_fn_19 },
    { 0x87089863U, (void *)tman_sync_fn_20 },
    { 0, NULL },
};

const TmanUidFunc g_uid_mbx_funcs[3] = {
    { 0xD310D2D9U, (void *)tman_sync_fn_25 },
    { 0x87089863U, (void *)tman_sync_fn_26 },
    { 0, NULL },
};

const TmanUidFunc g_uid_msgpipe_funcs[5] = {
    { 0x5BD49380U, (void *)tman_mem_fn_8 },
    { 0x85FE2B4CU, (void *)tman_mem_fn_7 },
    { 0xD310D2D9U, (void *)tman_mem_fn_5 },
    { 0x87089863U, (void *)tman_mem_fn_6 },
    { 0, NULL },
};

const TmanUidFunc g_uid_vpl_funcs[4] = {
    { 0x85FE2B4CU, (void *)tman_mem_fn_13 },
    { 0xD310D2D9U, (void *)tman_mem_fn_11 },
    { 0x87089863U, (void *)tman_mem_fn_12 },
    { 0, NULL },
};

const TmanUidFunc g_uid_fpl_funcs[3] = {
    { 0xD310D2D9U, (void *)tman_mem_fn_20 },
    { 0x87089863U, (void *)tman_mem_fn_21 },
    { 0, NULL },
};

const TmanUidFunc g_uid_tlspl_funcs[3] = {
    { 0xD310D2D9U, (void *)tman_mem_fn_26 },
    { 0x87089863U, (void *)tman_mem_fn_27 },
    { 0, NULL },
};

const TmanUidFunc g_uid_delay_funcs[3] = {
    { 0xD310D2D9U, (void *)tman_mem_fn_46 },
    { 0x87089863U, (void *)tman_mem_fn_47 },
    { 0, NULL },
};

const TmanUidFunc g_uid_alarm_funcs[3] = {
    { 0xD310D2D9U, (void *)tman_mem_fn_48 },
    { 0x87089863U, (void *)tman_mem_fn_49 },
    { 0, NULL },
};

const TmanUidFunc g_uid_vtimer_funcs[3] = {
    { 0xD310D2D9U, (void *)tman_mem_fn_50 },
    { 0x87089863U, (void *)tman_mem_fn_51 },
    { 0, NULL },
};

const TmanUidFunc g_uid_timer_funcs[3] = {
    { 0xD310D2D9U, (void *)tman_mem_fn_52 },
    { 0x87089863U, (void *)tman_mem_fn_53 },
    { 0, NULL },
};

const TmanUidFunc g_uid_thread_funcs[3] = {
    { 0xD310D2D9U, (void *)tman_thread_fn_29 },
    { 0x87089863U, (void *)tman_thread_fn_30 },
    { 0, NULL },
};

int sceKernelAllocateKTLS(int a1, int a2, int a3)
{
  int v3;
  int v4;
  int v6;
  int v9;
  int v10;
  u32 *v11;
  int v12;
  int ModuleGPByAddressForKernel;
  void (*v15)(int, u32 *, int, int, int, int, int);

  tman_cfc0(13);
  v6 = -2147352476;
  if ( v3 == 0 )
  {
    v9 = sceKernelCpuSuspendIntr(a1, a2);
    v10 = v9;
    v11 = &TMAN_BSS_U8(0x554);
    v12 = 0;
    while ( *v11 != 0 )
    {
      ++v12;
      v11 += 6;
      if ( v12 >= 16 )
      {
        sceKernelCpuResumeIntr(v9);
        return -2147352127;
      }
    }
    v11[1] = 0;
    v11[2] = a1;
    v11[3] = a2;
    v11[4] = a3;
    ModuleGPByAddressForKernel = sceKernelGetModuleGPByAddressForKernel(a2);
    v11[5] = ModuleGPByAddressForKernel;
    if ( ModuleGPByAddressForKernel == 0 )
      v11[5] = v4;
    v15 = (void (*)(int, u32 *, int, int, int, int, int))TMAN_BSS_U32(0x48);
    *v11 = 1;
    if ( v15 != NULL )
      v15(167, v11, 4, a1, a2, a3, v12);
    sceKernelCpuResumeIntr(v10);
    return v12;
  }
  return v6;
}


int sceKernelFreeKTLS(unsigned int a1, int a2)
{
  int v2;
  int v4;
  int v5;
  u32 *v6;

  v2 = -2147352476;
  v4 = a1 < 0x10;
  tman_cfc0(13);
  if ( a2 == 0 )
  {
    v2 = -2147352128;
    if ( v4 )
    {
      v5 = sceKernelCpuSuspendIntr(v4, -2147352576);
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((int (*)())TMAN_BSS_U32(0x48))(168, a1, 1, a1);
      v6 = (u32 *)((char *)&TMAN_BSS_U8(0x554) + 24 * a1);
      if ( *v6 != 0 )
      {
        if ( v6[1] != 0 )
        {
          sceKernelCpuResumeIntr(v5);
          return -2147352126;
        }
        else
        {
          *v6 = 0;
          sceKernelCpuResumeIntr(v5);
          return 0;
        }
      }
      else
      {
        sceKernelCpuResumeIntr(v5);
        return -2147352128;
      }
    }
  }
  return v2;
}


int sceKernelGetThreadKTLS(unsigned int a1, int a2, int a3)
{
  int v3;
  int v5;
  int v7;
  int v8;
  int v9;
  u32 *v10;
  int v11;
  int v12;
  int v13;
  int v15;
  int v16;
  int v17[4];

  tman_cfc0(13);
  v5 = a2;
  v7 = 0;
  if ( v3 == 0 && a1 < 0x10 )
  {
    v8 = sceKernelCpuSuspendIntr(0, a2);
    if ( v5 != 0 )
    {
      if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v5, TMAN_BSS_U32(0x50C), v17) != 0 )
        goto LABEL_16;
      v9 = v17[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
    }
    else
    {
      v5 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 8);
      v9 = (&TMAN_BSS_U32(0x40))[0];
    }
    v10 = (u32 *)((char *)&TMAN_BSS_U8(0x554) + 24 * a1);
    v11 = 4 * a1;
    if ( *v10 != 0 )
    {
      v12 = v11 + v9;
      v13 = *(u32 *)(v11 + v9 + 224);
      if ( v13 != 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((void (*)(int, int, int, unsigned int, int, int, int))TMAN_BSS_U32(0x48))(170, v13, 4, a1, v5, a3, v13);
        goto LABEL_9;
      }
      if ( a3 == 0 )
      {
LABEL_9:
        sceKernelCpuResumeIntr(v8);
        return v13;
      }
      v15 = sceKernelAllocHeapMemory(TMAN_BSS_U32(0x820), v10[2]);
      v13 = v15;
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((void (*)(int, int, int, unsigned int, int, int, int))TMAN_BSS_U32(0x48))(170, v15, 4, a1, v5, a3, v15);
      if ( v13 != 0 )
      {
        memset(v13, 0, v10[2]);
        v16 = v10[1];
        *(u32 *)(v12 + 224) = v13;
        v10[1] = v16 + 1;
        goto LABEL_9;
      }
    }
LABEL_16:
    sceKernelCpuResumeIntr(v8);
    return 0;
  }
  return v7;
}


int tman_thread_fn_1(int a1, int a2, int a3, int a4)
{
  int v4;
  int v6;
  int v7;
  u32 *v8;
  int v9;
  char *v10;
  int v12;
  int v13;
  int v14;
  int v15;

  v4 = 0;
  v6 = 0;
  v7 = 15;
  v8 = (u32 *)(a1 + 224);
  do
  {
    v9 = *v8;
    v10 = (char *)&TMAN_BSS_U8(0x554) + v6;
    if ( *v8 == 0 )
      goto LABEL_7;
    if ( *((u32 *)v10 + 3) != 0 )
    {
      if ( a1 != (&TMAN_BSS_U32(0x40))[0] )
      {
        v4 = 1;
LABEL_7:
        --v7;
        v6 += 24;
        goto LABEL_8;
      }
      (*((void (**)(u32, u32, int, int, unsigned int))v10 + 3))(
        *v8,
        *((u32 *)v10 + 4),
        a3,
        a4,
        0x20000u);
    }
    sceKernelFreeHeapMemory(TMAN_BSS_U32(0x820), v9, a3);
    --v7;
    v14 = sceKernelCpuSuspendIntr(v12, v13);
    v15 = *((u32 *)v10 + 1);
    *v8 = 0;
    *((u32 *)v10 + 1) = v15 - 1;
    sceKernelCpuResumeIntr(v14);
    v6 += 24;
LABEL_8:
    ++v8;
  }
  while ( v7 >= 0 );
  return v4;
}


int sceKernelGetKTLS(unsigned int a1)
{
  return sceKernelGetThreadKTLS(a1, 0, 1);
}


int module_bootstart(int a1, int a2)
{
  int v2;
  u32 v3;
  u32 *v4;
  int v5;
  const char *v6;
  int v7;
  int v8;
  int v9;
  u32 *BlockHeadAddr;
  int v11;
  int v12;
  u32 *v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  u32 *v21;
  int v22;
  int v23;
  u32 *v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int *v30;
  int v31;
  int v32;
  int v33;
  u32 *v34;
  int v35;
  int v37; // dc
  int v38;
  int v39;
  int v40;
  u32 v41[16];
  int v42;
  char v43[4];
  int v44;
  int v45;
  int *v46;

  v45 = sceKernelCpuSuspendIntr(a1, a2);
  v46 = &v39;
  sceKernelMemset32(TMAN_BSS_U32(0x40), 0, 2052);
  v3 = _mfc0(0x16u, 0);
  TMAN_BSS_U32(0x808) = v3;
  TMAN_BSS_U32(0x844) = sceKernelGetInitialRandomValue();
  tman_sched_fn_12();
  tman_thread_fn_14();
  tman_sched_fn_26();
  tman_sync_fn_2();
  tman_sync_fn_7();
  tman_sync_fn_13();
  tman_sync_fn_18();
  tman_sync_fn_23();
  tman_mem_fn_4();
  tman_mem_fn_10();
  tman_mem_fn_19();
  tman_mem_fn_25();
  tman_mem_fn_38();
  TMAN_BSS_U32(0x4DC) = (int)&TMAN_BSS_U32(0x4D8);
  v4 = &TMAN_BSS_U8(0xD8);
  v5 = 127;
  TMAN_BSS_U32(0x4E4) = (int)&TMAN_BSS_U32(0x4E0);
  TMAN_BSS_U32(0x4D8) = (int)&TMAN_BSS_U32(0x4D8);
  TMAN_BSS_U32(0x4F4) = (int)&TMAN_BSS_U32(0x4F0);
  TMAN_BSS_U32(0x4E0) = (int)&TMAN_BSS_U32(0x4E0);
  TMAN_BSS_U32(0x4FC) = (int)&TMAN_BSS_U32(0x4F8);
  TMAN_BSS_U32(0x4F0) = (int)&TMAN_BSS_U32(0x4F0);
  TMAN_BSS_U32(0x4EC) = (int)&TMAN_BSS_U32(0x4E8);
  TMAN_BSS_U32(0x4F8) = (int)&TMAN_BSS_U32(0x4F8);
  TMAN_BSS_U32(0x4E8) = (int)&TMAN_BSS_U32(0x4E8);
  do
  {
    --v5;
    *v4 = v4;
    v4[1] = v4;
    v4 += 2;
  }
  while ( v5 >= 0 );
  TMAN_BSS_U32(0x60) = -1;
  TMAN_BSS_U32(0x504) = (int)&TMAN_BSS_U32(0x500);
  TMAN_BSS_U32(0x78) = 0;
  TMAN_BSS_U32(0x9C) = 0;
  TMAN_BSS_U32(0x5C) = 0;
  TMAN_BSS_U32(0x838) = 0;
  TMAN_BSS_U32(0x824) = 0;
  TMAN_BSS_U32(0x500) = (int)&TMAN_BSS_U32(0x500);
  if ( sceKernelDipsw(23) != 0 )
  {
    (*(volatile u32 *)(0xBC000030U)) |= 0x300u;
    Kprintf("CPU Profiler enabled: ");
    if ( sceKernelDipsw(22) != 0 )
    {
      v6 = "Thread Mode\n";
      v7 = 1;
    }
    else
    {
      v6 = "Global Mode\n";
      v7 = 2;
    }
    TMAN_BSS_U32(0x824) = v7;
    Kprintf(v6);
  }
  else if ( sceKernelIsDevelopmentToolMode() != 0 && sceKernelDipsw(26) == 0 )
  {
    v37 = sceKernelDipsw(22) == 0;
    v38 = 4;
    if ( !v37 )
      v38 = 3;
    TMAN_BSS_U32(0x824) = v38;
  }
  TMAN_BSS_U32(0x820) = sceKernelCreateHeap(1, 2048, 1, "SceThreadman");
  sceKernelCreateUID(TMAN_BSS_U32(0x50C), "SceThreadmanIdle", 0, &v42);
  v8 = v42 + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
  *(u32 *)(v8 + 8) = *(u32 *)(v42 + 12) ^ TMAN_BSS_U32(0x844);
  *(u32 *)(v8 + 196) = 256;
  v9 = sceKernelAllocPartitionMemory(1, "stack:SceThreadmanIdle", 0, 256, 0);
  v44 = v9;
  *(u32 *)(v8 + 304) = v9;
  *(u32 *)(v8 + 316) = 1;
  BlockHeadAddr = (u32 *)sceKernelGetBlockHeadAddr(v9);
  *BlockHeadAddr = *(u32 *)(v8 + 8);
  *(u32 *)(v8 + 12) = 2;
  *(u32 *)(v8 + 188) = 127;
  *(u32 *)(v8 + 16) = 127;
  *(u32 *)(v8 + 20) = 0;
  *(u32 *)(v8 + 288) = 0;
  *(u32 *)(v8 + 216) = v2;
  v11 = TMAN_BSS_U32(0x824);
  *(u16 *)(v8 + 186) = 127;
  *(u32 *)(v8 + 192) = tman_idle_loop;
  *(u32 *)(v8 + 44) = BlockHeadAddr;
  *(u16 *)(v8 + 184) = 127;
  if ( v11 != 0 )
    *(u32 *)(v8 + 192) = (void *)tman_idle_halt;
  v39 = 8;
  v40 = 64;
  v12 = sceKernelAllocHeapMemoryWithOption(TMAN_BSS_U32(0x820), 320, &v39);
  *(u32 *)(v8 + 28) = v12;
  tman_thread_fn_8(v12);
  v13 = *(u32 **)(v8 + 28);
  v14 = *(u32 *)(v8 + 44);
  v13[9] = 3;
  v15 = *(u32 *)(v8 + 196);
  v13[47] = tman_thread_fn_19;
  v13[46] = v14 + v15;
  v13[45] = v14 + v15;
  v16 = *(u32 *)(v8 + 192);
  v17 = *(u32 *)(v8 + 216);
  TMAN_BSS_U32(0x54C) = v8;
  v13[14] = v16;
  v13[44] = v17;
  v13[13] = 3;
  tman_thread_fn_5(v8, v18, v14 + v15, v13);
  sceKernelCreateUID(TMAN_BSS_U32(0x50C), "SceThreadmanCleaner", 0, &v42);
  v19 = v42 + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
  *(u32 *)(v19 + 8) = *(u32 *)(v42 + 12) ^ TMAN_BSS_U32(0x844);
  *(u32 *)(v19 + 196) = 2048;
  v20 = sceKernelAllocPartitionMemory(1, "stack:SceThreadmanCleaner", 1, 2048, 0);
  v44 = v20;
  *(u32 *)(v19 + 316) = 1;
  *(u32 *)(v19 + 304) = v20;
  v21 = (u32 *)sceKernelGetBlockHeadAddr(v20);
  *v21 = *(u32 *)(v19 + 8);
  *(u32 *)(v19 + 12) = 4;
  *(u32 *)(v19 + 188) = 127;
  *(u32 *)(v19 + 16) = 1;
  *(u32 *)(v19 + 216) = v2;
  *(u32 *)(v19 + 20) = 0;
  *(u32 *)(v19 + 288) = 0;
  *(u32 *)(v19 + 192) = tman_thread_fn_3;
  *(u16 *)(v19 + 184) = 1;
  *(u16 *)(v19 + 186) = 1;
  v22 = TMAN_BSS_U32(0x820);
  *(u32 *)(v19 + 44) = v21;
  v39 = 8;
  v40 = 64;
  v23 = sceKernelAllocHeapMemoryWithOption(v22, 320, &v39);
  *(u32 *)(v19 + 28) = v23;
  tman_thread_fn_8(v23);
  v24 = *(u32 **)(v19 + 28);
  v25 = *(u32 *)(v19 + 44);
  v26 = *(u32 *)(v19 + 192);
  v24[9] = 3;
  v27 = *(u32 *)(v19 + 196);
  v24[47] = tman_thread_fn_19;
  v24[46] = v25 + v27;
  v24[45] = v25 + v27;
  v28 = *(u32 *)(v19 + 216);
  v29 = TMAN_BSS_U32(0x50C);
  v24[13] = 3;
  v24[14] = v26;
  v24[44] = v28;
  TMAN_BSS_U32(0x550) = v19;
  TMAN_BSS_U32(0x74) = 0;
  sceKernelCreateUID(v29, "SceThreadmanInit", 0, &v42);
  v30 = v46;
  v31 = v42 + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
  *(u32 *)(v31 + 8) = *(u32 *)(v42 + 12) ^ TMAN_BSS_U32(0x844);
  sceKernelQueryMemoryInfo(v30, v43, &v44);
  v32 = v44;
  *(u32 *)(v31 + 316) = 1;
  *(u32 *)(v31 + 304) = v32;
  v41[0] = 56;
  sceKernelQueryMemoryBlockInfo(v32, v41);
  v33 = *(u32 *)(v31 + 8);
  v34 = (u32 *)v41[10];
  *(u32 *)(v31 + 196) = v41[11];
  *v34 = v33;
  *(u32 *)(v31 + 188) = 127;
  *(u32 *)(v31 + 12) = 1;
  *(u32 *)(v31 + 216) = v2;
  *(u32 *)(v31 + 16) = 1;
  *(u32 *)(v31 + 20) = 0;
  *(u32 *)(v31 + 288) = 0;
  *(u16 *)(v31 + 186) = 32;
  *(u32 *)(v31 + 44) = v34;
  *(u16 *)(v31 + 184) = 32;
  v39 = 8;
  v40 = 64;
  v35 = sceKernelAllocHeapMemoryWithOption(TMAN_BSS_U32(0x820), 320, &v39);
  *(u32 *)(v31 + 28) = v35;
  tman_thread_fn_8(v35);
  (&TMAN_BSS_U32(0x40))[0] = v31;
  TMAN_BSS_U32(0x44) = v31;
  tman_ctc0(14, 0); tman_ctc0(15, 0);
  tman_ctc0(16, 0);
  InterruptManagerForKernel_E790EAED((void *)tman_eret, tman_thread_fn_6);
  tman_mem_fn_28();
  sceKernelRegisterIntrHandler(65, 0, tman_thread_fn_2, 0, 0);
  sceKernelEnableIntr(65);
  sceKernelRegisterExceptionHandler(11, (void *)tman_vfpu_exc_handler);
  tman_ctc0(8, 0);
  TMAN_BSS_U32(0x814) = sceKernelQuerySystemCall(sceKernelExitThread);
  TMAN_BSS_U32(0x818) = sceKernelQuerySystemCall(sceKernelReturnFromChangeStack);
  TMAN_BSS_U32(0x81C) = sceKernelQuerySystemCall(sceKernelReturnFromCallback);
  TMAN_BSS_U32(0x810) = sceKernelCreateEventFlag((int)"SceThreadmanSysStat", 512, 0, 0);
  sceKernelSetBootCallbackLevel(tman_thread_fn_18, 3, 0);
  sceKernelCpuResumeIntr(v45);
  sceKernelCpuEnableIntr();
  return 0;
}


int tman_thread_fn_2()
{
  return -1;
}


void tman_thread_fn_3(int a1, int a2)
{
  int v2;
  int v3;
  int *v4;
  int i;
  int v6;
  int *v7;
  int j;
  int v9;
  int *v10;
  int v11[4];

  while ( 1 )
  {
    v3 = sceKernelCpuSuspendIntr(a1, a2);
    v4 = &TMAN_BSS_U32(0x78);
    for ( i = TMAN_BSS_U32(0x9C); ; i = v4[9] )
    {
      v6 = 0;
      if ( i > 0 )
      {
        v7 = v4;
        for ( j = TMAN_BSS_U32(0x50C); ; j = TMAN_BSS_U32(0x50C) )
        {
          ++v6;
          if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v7[1], j, v11) == 0 )
            tman_thread_fn_12(v11[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21), v3);
          v9 = v7[1];
          ++v7;
          sceKernelDeleteUID(v9);
          if ( v6 >= v4[9] )
            break;
        }
      }
      v10 = v4;
      v4[9] = 0;
      v4 = (int *)*v4;
      *v10 = 0;
      if ( v10 != &TMAN_BSS_U32(0x78) )
        sceKernelFreeHeapMemory(TMAN_BSS_U32(0x820), v10, v2);
      if ( v4 == NULL )
        break;
    }
    *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 12) = 4;
    TMAN_BSS_U32(0x44) = 0;
    tman_sched_fn_19(1, 1);
  }
}


int tman_thread_fn_4(int a1)
{
  int *v1;
  int v3;
  int v4;
  int v5;
  int *v6;
  int j;
  int v8;
  int *v9;
  int v10;
  int *v11;
  int i;
  u32 *v14;

  v1 = &TMAN_BSS_U32(0x78);
  v3 = TMAN_BSS_U32(0x9C);
  v4 = TMAN_BSS_U32(0x9C);
  if ( TMAN_BSS_U32(0x9C) < 8 )
  {
LABEL_10:
    v10 = 0;
    if ( v3 <= 0 )
    {
LABEL_15:
      v1[v3 + 1] = a1;
      v8 = 0;
      v1[9] = v3 + 1;
      TMAN_BSS_U32(0x44) = 0;
      TMAN_BSS_U32(0x74) = 1;
    }
    else
    {
      v11 = v1 + 1;
      for ( i = v1[1]; ; i = *v11 )
      {
        ++v10;
        ++v11;
        v8 = -2147352168;
        if ( i == a1 )
          break;
        if ( v10 >= v3 )
          goto LABEL_15;
      }
    }
  }
  else
  {
    while ( 1 )
    {
      v5 = 0;
      if ( v4 > 0 )
        break;
LABEL_7:
      v9 = (int *)*v1;
      if ( *v1 == 0 )
      {
        v14 = (u32 *)sceKernelAllocHeapMemory(TMAN_BSS_U32(0x820), 40);
        *v1 = (int)v14;
        v8 = -2147352176;
        if ( v14 == NULL )
          return v8;
        *v14 = 0;
        v9 = (int *)*v1;
        *(u32 *)(*v1 + 36) = 0;
      }
      v3 = v9[9];
      v1 = v9;
      v4 = v3;
      if ( v3 < 8 )
        goto LABEL_10;
    }
    v6 = v1 + 1;
    for ( j = v1[1]; ; j = *v6 )
    {
      ++v5;
      ++v6;
      v8 = -2147352168;
      if ( j == a1 )
        break;
      if ( v5 >= v3 )
        goto LABEL_7;
    }
  }
  return v8;
}


unsigned int tman_thread_fn_5(u32 *a1)
{
  unsigned int v1;
  char *v3;
  int *v4;
  int v5;
  u32 *v6;

  v1 = a1[4];
  v3 = (char *)&TMAN_BSS_U8(0xD8) + 8 * v1;
  a1[1] = *((u32 *)v3 + 1);
  v4 = &(&TMAN_BSS_U32(0x40))[v1 >> 5];
  *((u32 *)v3 + 1) = a1;
  v5 = v4[34];
  v6 = (u32 *)a1[1];
  *a1 = v3;
  v4[34] = v5 | __builtin_rotateright32(0x80000000, v1);
  *v6 = a1;
  return 0x20000u;
}


int tman_thread_fn_6(int a1, int a2)
{
  int result;
  int v4;
  int v5;
  unsigned int v6;
  int v7;
  u32 *v8;
  int v9;
  int v10;
  unsigned int v11;
  unsigned int v12;
  int v13;
  int v14;
  s64 v15;
  s64 v16; // kr00_8
  u32 *v17;
  u32 *v18;
  int v19;
  int (*v20)(u32, u32, u32, u32);
  int v21;
  int v22;
  int v23;
  int v24;
  u32 *v25;
  int v26;
  int v27;
  int v28;
  int v29;
  unsigned int v30;
  u32 *v31;
  int v32;
  int v33;
  int v34;
  int v35;

  result = a1;
  if ( TMAN_BSS_U32(0x4C) == 0 )
  {
    v4 = (&TMAN_BSS_U32(0x40))[0];
    v5 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 60);
    v6 = 0xBC400000;
    if ( v5 != 0 )
    {
      v31 = (u32 *)(v5 + 16);
      v32 = -1136656384;
      do
      {
        v33 = *(u32 *)(v32 + 4);
        v34 = *(u32 *)(v32 + 8);
        v35 = *(u32 *)(v32 + 12);
        *v31 = *(u32 *)v32;
        v32 += 16;
        v31 += 4;
        *(v31 - 3) = v33;
        *(v31 - 2) = v34;
        *(v31 - 1) = v35;
      }
      while ( v32 != -1136656304 );
      *v31 = (*(volatile u32 *)(0xBC400050U));
      v7 = *(u32 *)(v4 + 12);
    }
    else
    {
      v7 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 12);
    }
    if ( v7 != 32 )
    {
      if ( *(int *)(v4 + 20) < 0 )
      {
        v9 = *(u32 *)(v4 + 28);
        if ( (*(u32 *)(v9 + 52) & 0x18) != 0 )
        {
          v11 = *(u32 *)(v9 + 180);
          v12 = *(u32 *)(v4 + 40);
        }
        else
        {
          v10 = *(u32 *)(v4 + 36);
          v12 = *(u32 *)(v9 + 180);
          v11 = 0xFFFFFFFF;
          if ( v10 != 0 )
            v11 = *(u32 *)(v10 + 8);
        }
        v13 = *(u32 *)(v4 + 8);
        if ( **(u32 **)(v4 + 44) != v13 )
        {
          tman_thread_fn_22(v4, 0, v11);
          v13 = *(u32 *)(v4 + 8);
        }
        if ( **(u32 **)(v4 + 48) != v13 )
          tman_thread_fn_22(v4, 1, v12);
        if ( v11 < *(u32 *)(v4 + 44) )
          tman_thread_fn_22(v4, 0, v11);
        v6 = *(u32 *)(v4 + 48);
        if ( v12 < v6 )
          tman_thread_fn_22(v4, 1, v12);
        if ( *(u32 *)(v4 + 220) != *(u32 *)(*(u32 *)(v4 + 28) + 168) )
          tman_thread_fn_23(v4);
      }
      else
      {
        v8 = *(u32 **)(v4 + 44);
        v29 = *(u32 *)(v4 + 28);
        v30 = *(u32 *)(v29 + 180);
        if ( *v8 != *(u32 *)(v4 + 8) )
        {
          tman_thread_fn_22(v4, 1, *(u32 *)(v29 + 180));
          v8 = *(u32 **)(v4 + 44);
        }
        if ( v30 < (unsigned int)v8 )
          tman_thread_fn_22(v4, 1, v30);
      }
    }
    v14 = TMAN_BSS_U32(0x44);
    if ( TMAN_BSS_U32(0x44) == 0 )
    {
      tman_thread_fn_20(v6);
      v14 = TMAN_BSS_U32(0x44);
    }
    if ( v4 == v14 )
    {
      ++TMAN_BSS_U32(0x54);
    }
    else
    {
      v15 = ((u64)(v15) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
      v16 = *(u64 *)(v4 + 52) + v15 - TMAN_BSS_U64(0xB0);
      TMAN_BSS_U64(0xB0) = v15;
      *(u64 *)(v4 + 52) = v16;
      v17 = *(u32 **)(v4 + 64);
      v18 = (u32 *)TMAN_BSS_U32(0x44);
      ++TMAN_BSS_U32(0x50);
      v19 = *(u32 *)(TMAN_BSS_U32(0x44) + 28);
      ++*v17;
      if ( (*(u32 *)(v19 + 52) & 0x40000000) != 0 )
      {
        if ( TMAN_BSS_U32(0x5C) != v18[8] )
          a2 |= 0x100u;
        v20 = TMAN_BSS_U32(0x48);
      }
      else
      {
        v20 = TMAN_BSS_U32(0x48);
      }
      if ( v20 != NULL )
        ((void (*)(int, u32, int, u32, u32, u32, u32, int))v20)(
          1,
          0,
          5,
          *(u32 *)(v4 + 8),
          *(u32 *)(v4 + 16),
          v18[2],
          v18[4],
          a2);
      v21 = TMAN_BSS_U32(0x44);
      if ( *(u32 *)tman_idle_loop != 0 )
        *(u32 *)tman_idle_loop = *(u32 *)(TMAN_BSS_U32(0x44) + 220);
      v4 = v21;
      (&TMAN_BSS_U32(0x40))[0] = v21;
    }
    v22 = *(u32 *)(v4 + 60);
    if ( v22 != 0 )
    {
      v24 = -1136656384;
      v25 = (u32 *)(v22 + 16);
      do
      {
        v26 = v25[1];
        v27 = v25[2];
        v28 = v25[3];
        *(u32 *)v24 = *v25;
        v25 += 4;
        v24 += 16;
        *(u32 *)(v24 - 12) = v26;
        *(u32 *)(v24 - 8) = v27;
        *(u32 *)(v24 - 4) = v28;
      }
      while ( v25 != (u32 *)(v22 + 96) );
      *(u32 *)v24 = *v25;
      _sync();
      tman_ctc0(25, 0);
      v23 = *(u32 *)(v4 + 28);
    }
    else
    {
      tman_ctc0(25, 0);
      v23 = *(u32 *)(v4 + 28);
    }
    if ( (*(u32 *)(v23 + 52) & 0x40000000) != 0 )
      tman_thread_fn_7(v4);
    tman_ctc0(14, 0);
    tman_ctc0(15, 0);
    tman_ctc0(16, 0);
    return *(u32 *)(v4 + 28);
  }
  return result;
}


void tman_thread_fn_7(u32 *a1)
{
  float a2 = 0, a3 = 0, a4 = 0, a5 = 0, a6 = 0, a7 = 0, a8 = 0, a9 = 0;
  int v10;
  unsigned int v11;

  tman_cfc0(8);
  if ( a1 != NULL && (u32 *)a1[7] != a1 )
    tman_sched_fn_1((int)a1, a2, a3, a4, a5, a6, a7, a8, a9);
  if ( TMAN_BSS_U32(0x5C) != a1[8] )
  {
    v10 = a1[5] >> 31;
    if ( TMAN_BSS_U32(0x60) != v10 )
    {
      tman_vfpu_flush();
      if ( v10 != 0 )
        v11 = (*(volatile u32 *)(0xBC000044U)) & 0xFFFFFFDF;
      else
        v11 = (*(volatile u32 *)(0xBC000044U)) | 0x20;
      (*(volatile u32 *)(0xBC000044U)) = v11;
      _sync();
      TMAN_BSS_U32(0x60) = v10;
    }
    if ( TMAN_BSS_U32(0x5C) != 0 )
      tman_vfpu_save((u32 *)TMAN_BSS_U32(0x5C));
    TMAN_BSS_U32(0x5C) = a1[8];
    tman_vfpu_restore(TMAN_BSS_U32(0x5C));
    ++TMAN_BSS_U32(0x58);
  }
}


int sceKernelCreateThread(const char *a1, int a2, int a3, unsigned int a4, int a5, int *a6)
{
  int v6;
  int v7;
  int v8;
  int v10;
  int v14;
  int v15;
  int v17;
  unsigned int v19;
  int v20;
  unsigned int v21;
  int v22;
  int MemoryPartitionInfo;
  int v24;
  int v25;
  int v26;
  int ModuleGPByAddressForKernel;
  int v28;
  unsigned int v29;
  unsigned int v30;
  int v31;
  int v32;
  int BlockHeadAddr;
  int v34;
  int v35;
  int v36;
  int *v37;
  int v38;
  int v39;
  int v40;
  int v41;
  int v42;
  int v43;
  int v44;
  int v45;
  u32 *v46;
  unsigned int v47;
  int v48;
  int v49;
  int v50;

  v8 = v6 << 11;
  v10 = (v6 << 11) & (unsigned int)a1;
  v14 = v6 << 11;
  v15 = a5;
  if ( v10 < 0 )
    return -2147352365;
  v17 = v8 & a2;
  if ( (v8 & a2) < 0 || (((unsigned int)(a6 + 2) | (unsigned int)a6) & v8 & 0x80000000) != 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( v8 != 0 || *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 136) != 0 )
    return -2147352476;
  if ( (a5 & 0x70F9F00) != 0 )
    return -2147352175;
  v19 = a3 - 1;
  if ( v14 < 0 )
  {
    v20 = a5 & 0x7FFFFFF;
    if ( (a5 & 0x800000) != 0 )
    {
      v20 = a5 & 0x77FFFFF;
      v47 = 0x80000000;
    }
    else
    {
      v47 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 20) & 0xF0000000;
    }
    v15 = v20 | v47;
    v19 = a3 - 1;
  }
  if ( v19 >= 0x7E )
    return -2147352173;
  v21 = a3 - 8;
  if ( v15 < 0 )
  {
    v17 = v21 < 0x70;
    v15 |= 0xFFu;
    if ( v21 >= 0x70 )
      return -2147352173;
  }
  if ( (a2 & 3) != 0 )
    return -2147352174;
  if ( a4 < 0x200 )
    return -2147352172;
  v49 = sceKernelCpuSuspendIntr(v17, v21);
  TMAN_BSS_U32(0x72C) = 0;
  TMAN_BSS_U32(0x730) = 0;
  if ( a6 != NULL )
  {
    v22 = *a6;
    if ( (unsigned int)*a6 >= 9 )
      v22 = 8;
    memcpy(&TMAN_BSS_U32(0x72C), a6, v22);
  }
  MemoryPartitionInfo = sceKernelCreateUID(TMAN_BSS_U32(0x50C), a1, (u8)(v15 | (v14 >> 31)), &v48);
  if ( MemoryPartitionInfo != 0 )
  {
LABEL_58:
    sceKernelCpuResumeIntr(v49);
    return MemoryPartitionInfo;
  }
  v24 = *(u32 *)(v48 + 12);
  v25 = TMAN_BSS_U32(0x844);
  v26 = v48 + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
  *(u32 *)(v26 + 300) = a6;
  *(u32 *)(v26 + 188) = 127;
  *(u32 *)(v26 + 16) = a3;
  *(u32 *)(v26 + 20) = v15;
  *(u32 *)(v26 + 288) = v15;
  *(u32 *)(v26 + 192) = a2;
  *(u16 *)(v26 + 184) = a3;
  *(u16 *)(v26 + 186) = a3;
  v50 = v24 ^ v25;
  ModuleGPByAddressForKernel = sceKernelGetModuleGPByAddressForKernel(a2);
  *(u32 *)(v26 + 216) = ModuleGPByAddressForKernel;
  if ( ModuleGPByAddressForKernel == 0 )
    *(u32 *)(v26 + 216) = v7;
  if ( *(int *)(v26 + 20) >= 0 && a2 >= 0 )
  {
    sceKernelDeleteUID(v50);
    sceKernelCpuResumeIntr(v49);
    return -2147352365;
  }
  snprintf(&TMAN_BSS_U32(0x734), 31, "stack:%s", a1);
  v28 = TMAN_BSS_U32(0x730);
  v29 = (a4 + 255) & 0xFFFFFF00;
  if ( TMAN_BSS_U32(0x730) == 0 )
  {
    v28 = 2;
    if ( *(int *)(v26 + 20) >= 0 )
      v28 = 1;
  }
  TMAN_BSS_U32(0x714) = 16;
  MemoryPartitionInfo = sceKernelQueryMemoryPartitionInfo(v28, &TMAN_BSS_U32(0x714));
  if ( MemoryPartitionInfo != 0 )
  {
    sceKernelDeleteUID(v50);
    goto LABEL_58;
  }
  if ( *(int *)(v26 + 20) >= 0 )
  {
    v30 = (unsigned int)v15 >> 22;
    if ( (TMAN_BSS_U32(0x720) & 3) == 0 )
      goto LABEL_33;
LABEL_54:
    sceKernelDeleteUID(v50);
    sceKernelCpuResumeIntr(v49);
    return -2147352367;
  }
  v30 = (unsigned int)v15 >> 22;
  if ( (TMAN_BSS_U32(0x720) & 3) != 3 )
    goto LABEL_54;
LABEL_33:
  v31 = sceKernelAllocPartitionMemory(v28, &TMAN_BSS_U32(0x734), (v30 ^ 1) & 1, v29, 0);
  if ( v31 <= 0 )
    goto LABEL_47;
  v32 = *(u32 *)(v26 + 20);
  *(u32 *)(v26 + 304) = v31;
  *(u32 *)(v26 + 312) = v28;
  if ( v32 < 0 )
    *(u32 *)(v26 + 316) = 1;
  else
    *(u32 *)(v26 + 316) = v28;
  *(u32 *)(v26 + 196) = v29;
  BlockHeadAddr = sceKernelGetBlockHeadAddr(v31);
  v34 = *(u32 *)(v26 + 20);
  *(u32 *)(v26 + 44) = BlockHeadAddr;
  if ( v34 < 0 )
  {
    snprintf(&TMAN_BSS_U32(0x734), 31, "kstack:%s", a1);
    v41 = sceKernelAllocPartitionMemory(1, &TMAN_BSS_U32(0x734), 1, 2048, 0);
    if ( v41 > 0 )
    {
      *(u32 *)(v26 + 204) = 2048;
      *(u32 *)(v26 + 308) = v41;
      v42 = sceKernelGetBlockHeadAddr(v41);
      v43 = TMAN_BSS_U32(0x824) == 1 || TMAN_BSS_U32(0x824) == 3;
      *(u32 *)(v26 + 48) = v42;
      if ( !v43 )
        goto LABEL_37;
      v44 = sceKernelAllocHeapMemory(TMAN_BSS_U32(0x820), 100);
      *(u32 *)(v26 + 60) = v44;
      v45 = v44 - 100;
      if ( v44 != 0 )
      {
        do
        {
          v45 += 4;
          *(u32 *)(v45 + 96) = 0;
        }
        while ( v45 != v44 );
        v46 = *(u32 **)(v26 + 60);
        v46[2] = 1;
        *v46 = -1136656384;
        v46[1] = 1;
        goto LABEL_37;
      }
    }
LABEL_47:
    sceKernelDeleteUID(v50);
    sceKernelCpuResumeIntr(v49);
    return -2147352176;
  }
LABEL_37:
  TMAN_BSS_U32(0x728) = 64;
  TMAN_BSS_U32(0x724) = 8;
  v35 = sceKernelAllocHeapMemoryWithOption(TMAN_BSS_U32(0x820), 320, &TMAN_BSS_U32(0x724));
  *(u32 *)(v26 + 28) = v35;
  if ( v35 == 0 )
    goto LABEL_47;
  if ( (v15 & 0x4000) != 0 )
  {
    TMAN_BSS_U32(0x728) = 64;
    TMAN_BSS_U32(0x724) = 8;
    v36 = sceKernelAllocHeapMemoryWithOption(TMAN_BSS_U32(0x820), 576, &TMAN_BSS_U32(0x724));
    *(u32 *)(v26 + 32) = v36;
    if ( v36 == 0 )
      goto LABEL_47;
  }
  *(u32 *)(v26 + 4) = TMAN_BSS_U32(0x4EC);
  TMAN_BSS_U32(0x4EC) = v26;
  v37 = *(int **)(v26 + 4);
  *(u32 *)v26 = &TMAN_BSS_U32(0x4E8);
  v38 = v49;
  *(u32 *)(v26 + 84) = -2147352158;
  *(u32 *)(v26 + 12) = 16;
  *v37 = v26;
  sceKernelCpuResumeIntr(v38);
  if ( (v15 & 0x100000) == 0 )
  {
    sceKernelMemset32(*(u32 *)(v26 + 44), -1, *(u32 *)(v26 + 196));
    if ( *(int *)(v26 + 20) < 0 )
      sceKernelMemset32(*(u32 *)(v26 + 48), -1, *(u32 *)(v26 + 204));
  }
  v49 = sceKernelCpuSuspendIntr(v39, v40);
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(16, 64, 2, a2);
  tman_sched_fn_20(1, v26, v49);
  sceKernelCpuResumeIntr(v49);
  return v50;
}


int sceKernelDeleteThread(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v6;
  int v7;
  int v8;
  int *v9;
  int v10;
  int v11;
  u32 v13[4];

  tman_cfc0(13);
  v4 = v3 << 11;
  if ( v2 != 0 || *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 136) != 0 )
    return -2147352476;
  if ( a1 == 0 )
    return -2147352156;
  v6 = sceKernelCpuSuspendIntr(0x20000u, a2);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x50C), v13) == 0 )
  {
    v7 = *(u16 *)(v13[0] + 22) & 0x10;
    if ( v4 >= 0 )
      v7 = 16;
    if ( v7 == 0 )
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352367;
    }
    v8 = *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
    v9 = (int *)(v13[0] + 4 * v8);
    if ( v9 != (int *)TMAN_BSS_U32(0x54C) && v9 != (int *)TMAN_BSS_U32(0x550) )
    {
      if ( v9[3] != 0x10 && v9[3] != 0x20 )
      {
        sceKernelCpuResumeIntr(v6);
        return -2147352156;
      }
      if ( v9[34] != 0 )
      {
        sceKernelCpuResumeIntr(v6);
        return -2147352476;
      }
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((int (*)())TMAN_BSS_U32(0x48))(17, v13[0] + 4 * v8, 1, a1);
      tman_sched_fn_20(8, (int)v9, v6);
      tman_sync_fn_6((int)v9);
      tman_sync_fn_12((int)v9);
      v10 = *v9;
      v11 = v9[1];
      v9[3] = 0;
      *(u32 *)(v10 + 4) = v11;
      *(u32 *)v9[1] = v10;
      tman_thread_fn_12(v9, v6);
      sceKernelDeleteUID(v9[2]);
      sceKernelCpuResumeIntr(v6);
      return 0;
    }
  }
  sceKernelCpuResumeIntr(v6);
  return -2147352168;
}


u32 *tman_thread_fn_8(u32 *a1)
{
  int v1;
  u32 *v2;
  u32 *result;
  int i;

  v1 = 31;
  v2 = a1 + 16;
  do
  {
    --v1;
    *v2++ = -559038737;
  }
  while ( v1 >= 0 );
  a1[42] = 0;
  a1[43] = 0;
  result = a1 + 48;
  for ( i = 31; i >= 0; --i )
    *result++ = 2139095041;
  a1[10] = 3584;
  a1[12] = -559038737;
  a1[15] = 0;
  a1[11] = -559038737;
  a1[13] = 0;
  a1[14] = 0;
  a1[16] = 0;
  return result;
}


int sceKernelStartThread(int a1, int a2, int a3, int a4, int a5)
{
  int v5;
  int v8;
  int v9;
  int v11;
  int v12;
  int v13;
  int v14;
  u32 *v15;
  int *v16;
  int v17;
  int v18;
  u32 *v19;
  u32 *v20;
  int v21;
  int v22;
  int v23;
  int v24;
  int v25;
  int v26;
  int *v27;
  int v28;
  int *v29;
  int v31;
  int v32;
  int *v33;
  int v34;
  int v35;
  int **v36;
  int *v37;
  u32 *v38;
  int v39[4];

  v8 = (v5 << 11) & ((a3 + a2) | a3 | a2);
  v9 = v5 << 11;
  if ( v8 < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a5 != 0 || *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 136) != 0 )
    return -2147352476;
  if ( a1 == 0 )
    return -2147352169;
  v11 = sceKernelCpuSuspendIntr(a1, v8);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x50C), v39) != 0 )
  {
    sceKernelCpuResumeIntr(v11);
    return -2147352168;
  }
  else
  {
    v12 = *(u16 *)(v39[0] + 22) & 2;
    if ( v9 >= 0 )
      v12 = 2;
    if ( v12 != 0 )
    {
      v13 = 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
      v14 = v39[0] + v13;
      if ( *(u32 *)(v39[0] + v13 + 12) == 16 )
      {
        tman_thread_fn_13(v39[0] + v13, v39[0]);
        if ( *(u32 *)(v14 + 136) == 0 )
        {
          if ( TMAN_BSS_U32(0x48) != NULL )
            ((void (*)(int, int, int, int, int, int))TMAN_BSS_U32(0x48))(18, v14, 3, a1, a2, a3);
          v15 = *(u32 **)(v14 + 44);
          v16 = (u32 *)((char *)v15 + (*(u32 *)(v14 + 196) & 0xFFFFFFFC));
          if ( *(int *)(v14 + 20) < 0 )
          {
            v16 -= 64;
            *(u32 *)(v14 + 220) = v16;
            v37 = v16 - 64;
            do
            {
              ++v37;
              v37[63] = 0;
            }
            while ( v37 != v16 );
            v38 = *(u32 **)(v14 + 220);
            v38[63] = -1;
            v38[48] = a1;
            v38[50] = v15;
            v38[62] = -1;
          }
          *v15 = *(u32 *)(v14 + 8);
          if ( *(int *)(v14 + 20) < 0 )
            **(u32 **)(v14 + 48) = *(u32 *)(v14 + 8);
          v17 = v14 + 160;
          do
          {
            v17 += 4;
            *(u32 *)(v17 + 60) = 0;
          }
          while ( v17 != v14 + 224 );
          if ( a3 == 0 || a2 == 0 )
          {
            *(u32 *)(v14 + 292) = 0;
            *(u32 *)(v14 + 296) = 0;
          }
          else
          {
            v16 = (int *)((char *)v16 - ((a2 + 15) & 0xFFFFFFF0));
            memcpy(v16, a3, a2);
            *(u32 *)(v14 + 292) = a2;
            *(u32 *)(v14 + 296) = v16;
          }
          if ( *(int *)(v14 + 20) < 0 )
          {
            v16 -= 16;
            *v16 = (TMAN_BSS_U32(0x814) << 6) & 0x3FFFFC0 | 0xC;
            v16[1] = 13;
            v16[2] = 268500991;
            v16[3] = 0;
            _cache(0x1Bu, v16);
            _cache(8u, v16);
            _sync();
          }
          v18 = *(s16 *)(v14 + 184);
          v19 = *(u32 **)(v14 + 28);
          *(u32 *)(v14 + 20) = *(u32 *)(v14 + 288);
          *(u32 *)(v14 + 188) = 127;
          *(u32 *)(v14 + 16) = v18;
          *(u32 *)(v14 + 88) = 0;
          *(u32 *)(v14 + 92) = 0;
          *(u32 *)(v14 + 80) = 0;
          *(u16 *)(v14 + 186) = v18;
          tman_thread_fn_8(v19);
          v20 = *(u32 **)(v14 + 28);
          v21 = *(u32 *)(v14 + 220);
          *(u32 *)(v14 + 200) = v16;
          v20[9] = 3;
          v20[42] = v21;
          if ( *(int *)(v14 + 20) >= 0 )
          {
            *(u32 *)(v14 + 40) = 0;
          }
          else
          {
            v16 = (int *)(*(u32 *)(v14 + 48) + *(u32 *)(v14 + 204));
            *(u32 *)(v14 + 40) = v16;
          }
          v20[45] = v16;
          v22 = *(u32 *)v14;
          v23 = *(u32 *)(v14 + 4);
          v20[46] = v20[45];
          v24 = *(u32 *)(v14 + 216);
          v20[14] = tman_thread_fn_9;
          v20[44] = v24;
          v20[13] = 3;
          *(u32 *)(v22 + 4) = v23;
          *(u32 *)(v14 + 84) = -2147352156;
          v25 = TMAN_BSS_U32(0x70);
          **(u32 **)(v14 + 4) = v22;
          if ( v25 != 0 )
          {
            v26 = *(u32 *)(v14 + 20);
            if ( v26 < 0 || (v27 = (int *)(&TMAN_BSS_U32(0x40))[0], (v26 & 0x8000000) != 0) )
            {
              v28 = TMAN_BSS_U32(0x4F4);
              *(u32 *)v14 = &TMAN_BSS_U32(0x4F0);
              *(u32 *)(v14 + 4) = v28;
              TMAN_BSS_U32(0x4F4) = v14;
              v29 = *(int **)(v14 + 4);
              *(u32 *)(v14 + 12) = 8;
              *v29 = v14;
LABEL_29:
              sceKernelCpuResumeIntr(v11);
              return 0;
            }
          }
          else
          {
            v27 = (int *)(&TMAN_BSS_U32(0x40))[0];
          }
          v31 = v27[4];
          if ( *(u32 *)(v14 + 16) >= v31 || TMAN_BSS_U32(0x4C) != 0 )
          {
            *(u32 *)(v14 + 12) = 2;
            tman_thread_fn_5((u32 *)v14);
          }
          else
          {
            if ( v11 != 0 )
            {
              v32 = (&TMAN_BSS_U32(0x40))[2 * v31 + 38];
              v33 = &(&TMAN_BSS_U32(0x40))[(unsigned int)v31 >> 5];
              v27[1] = *(u32 *)(v32 + 4);
              v34 = __builtin_rotateright32(0x80000000, v31);
              *(u32 *)(v32 + 4) = v27;
              v35 = v33[34];
              v36 = (int **)v27[1];
              TMAN_BSS_U32(0x44) = v14;
              v27[3] = 2;
              *v27 = v32;
              v33[34] = v35 | v34;
              *(u32 *)(v14 + 12) = 1;
              *v36 = v27;
            }
            else
            {
              *(u32 *)(v14 + 12) = 2;
              tman_thread_fn_5((u32 *)v14);
              TMAN_BSS_U32(0x44) = 0;
            }
            tman_sched_fn_19(0, v11);
          }
          goto LABEL_29;
        }
        sceKernelCpuResumeIntr(v11);
        return -2147352476;
      }
      sceKernelCpuResumeIntr(v11);
      return -2147352156;
    }
    else
    {
      sceKernelCpuResumeIntr(v11);
      return -2147352367;
    }
  }
}


u32 sceKernelExitThread(int a1)
{
  int v1;
  u32 InterruptMask;
  int *v4;
  int CompiledSdkVersion;
  int v6;
  void (*v7)(int, int, int, int);

  InterruptMask = 0x80020064;
  tman_cfc0(13);
  if ( v1 == 0 )
  {
    InterruptMask = 0x80020064;
    v4 = TMAN_BSS_U32(0x40);
    if ( *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 136) == 0 )
    {
      if ( (InterruptMask = getInterruptMask()) != 0 && TMAN_BSS_U32(0x4C) == 0
        || (CompiledSdkVersion = sceKernelGetCompiledSdkVersion(TMAN_BSS_U32(0x40)),
            v4 = (int *)(CompiledSdkVersion > 50855935),
            InterruptMask = 0x800201A7,
            CompiledSdkVersion <= 50855935) )
      {
        if ( a1 < 0 )
          a1 = -2147352366;
        v6 = sceKernelCpuSuspendIntr(v4, InterruptMask);
        v7 = (void (*)(int, int, int, int))TMAN_BSS_U32(0x48);
        *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 84) = a1;
        if ( v7 != NULL )
          v7(19, a1, 1, a1);
        tman_thread_fn_24(0, v6, a1);
      }
    }
  }
  return InterruptMask;
}


u32 sceKernelExitDeleteThread(int a1)
{
  int v1;
  u32 InterruptMask;
  int *v4;
  int CompiledSdkVersion;
  int v6;
  void (*v7)(int, int, int, int);
  int v8;

  InterruptMask = 0x80020064;
  tman_cfc0(13);
  if ( v1 == 0 )
  {
    InterruptMask = 0x80020064;
    v4 = TMAN_BSS_U32(0x40);
    if ( *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 136) == 0 )
    {
      if ( (InterruptMask = getInterruptMask()) != 0 && TMAN_BSS_U32(0x4C) == 0
        || (CompiledSdkVersion = sceKernelGetCompiledSdkVersion(TMAN_BSS_U32(0x40)),
            v4 = (int *)(CompiledSdkVersion > 50855935),
            InterruptMask = 0x800201A7,
            CompiledSdkVersion <= 50855935) )
      {
        if ( a1 < 0 )
          a1 = -2147352366;
        v6 = sceKernelCpuSuspendIntr(v4, InterruptMask);
        v7 = (void (*)(int, int, int, int))TMAN_BSS_U32(0x48);
        v8 = (&TMAN_BSS_U32(0x40))[0];
        *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 84) = a1;
        if ( v7 != NULL )
        {
          v7(20, a1, 1, a1);
          v8 = (&TMAN_BSS_U32(0x40))[0];
        }
        tman_thread_fn_24(1, v6, *(u32 *)(v8 + 84));
      }
    }
  }
  return InterruptMask;
}


int sceKernelTerminateThread(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int UIDcontrolBlockWithType;
  int v7;
  int v8;
  int v9;
  int v10;
  int CompiledSdkVersion;
  u32 v13[4];

  v4 = a1;
  v5 = v3 << 11;
  if ( !a1 )
    return -2147352169;
  tman_cfc0(13);
  if ( v2 != 0 )
  {
    CompiledSdkVersion = sceKernelGetCompiledSdkVersion(a1);
    a2 = 50855935;
    a1 = CompiledSdkVersion > 50855935;
    if ( CompiledSdkVersion > 50855935 )
      return -2147352476;
  }
  if ( TMAN_BSS_U32(0x4C) != 0 && sceKernelGetCompiledSdkVersion(a1) > 50855935 )
    return -2147352153;
  v7 = sceKernelCpuSuspendIntr(a1, a2);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(v4, TMAN_BSS_U32(0x50C), v13);
  v8 = v13[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352168;
  }
  else
  {
    v9 = *(u16 *)(v13[0] + 22) & 2;
    if ( v5 >= 0 )
      v9 = 2;
    if ( v9 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((int (*)())TMAN_BSS_U32(0x48))(21, v7, 1, *(u32 *)(v13[0] + 12) ^ TMAN_BSS_U32(0x844));
        v8 = v13[0];
      }
      v10 = tman_thread_fn_25(v8, 0, v7);
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


int sceKernelTerminateDeleteThread(int a1, int a2)
{
  int v2;
  int v3;
  int v5;
  int CompiledSdkVersion;
  int UIDcontrolBlockWithType;
  int v8;
  int v9;
  int v10;
  int v11;
  u32 v13[4];

  v3 = v2 << 11;
  if ( a1 == 0 )
    return -2147352169;
  v5 = TMAN_BSS_U32(0x4C);
  if ( TMAN_BSS_U32(0x4C) != 0 )
  {
    CompiledSdkVersion = sceKernelGetCompiledSdkVersion(TMAN_BSS_U32(0x4C));
    a2 = CompiledSdkVersion > 50855935;
    if ( CompiledSdkVersion > 50855935 )
      return -2147352153;
  }
  v8 = sceKernelCpuSuspendIntr(v5, a2);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x50C), v13);
  v9 = v13[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v8);
    return -2147352168;
  }
  else
  {
    v10 = *(u16 *)(v13[0] + 22) & 2;
    if ( v3 >= 0 )
      v10 = 2;
    if ( v10 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((int (*)())TMAN_BSS_U32(0x48))(22, v8, 1, *(u32 *)(v13[0] + 12) ^ TMAN_BSS_U32(0x844));
        v9 = v13[0];
      }
      v11 = tman_thread_fn_25(v9, 1, v8);
      sceKernelCpuResumeIntr(v8);
      return v11;
    }
    else
    {
      sceKernelCpuResumeIntr(v8);
      return -2147352367;
    }
  }
}


int tman_thread_fn_9(int a1, int a2)
{
  int v2;
  int v3;
  float v4;
  float v5;
  float v6;
  float v7;
  float v8;
  float v9;
  float v10;
  float v11;
  u32 *v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int (*v17)();
  int v18;
  int v19;
  int v20;
  u32 *v21;
  u32 *v22;
  int i;
  int v24;

  v3 = (&TMAN_BSS_U32(0x40))[0];
  v2 = sceKernelCpuSuspendIntr(a1, a2);
  tman_sched_fn_20(2, v3, v2);
  tman_thread_fn_8(*(u32 **)(v3 + 28));
  v12 = *(u32 **)(v3 + 28);
  v13 = *(u32 *)(v3 + 296);
  v12[9] = 3;
  v14 = *(u32 *)(v3 + 292);
  v15 = *(u32 *)(v3 + 220);
  v12[21] = v13;
  v12[20] = v14;
  v12[42] = v15;
  v16 = *(u32 *)(v3 + 200);
  v12[46] = v16;
  v12[45] = v16;
  v17 = tman_thread_fn_19;
  if ( *(int *)(v3 + 20) < 0 )
    v17 = *(int (**)())(v3 + 200);
  v12[47] = v17;
  v12[44] = *(u32 *)(v3 + 216);
  v18 = 19;
  if ( *(int *)(v3 + 20) >= 0 )
    v18 = 3;
  v19 = *(u32 *)(v3 + 192);
  v12[13] = v18;
  v12[14] = v19;
  v20 = *(u32 *)(v3 + 20);
  if ( v20 < 0 )
  {
    *(u32 *)(v15 + 196) = 0;
    v20 = *(u32 *)(v3 + 20);
  }
  v21 = *(u32 **)(v3 + 32);
  *(u32 *)(v3 + 20) = v20 & 0xFFFFBFFF | *(u32 *)(v3 + 288) & 0x4000;
  if ( v21 != NULL )
  {
    v22 = v21;
    for ( i = 127; i >= 0; --i )
      *v22++ = 2139095041;
    v24 = TMAN_BSS_U32(0x5C);
    v21[129] = 228;
    v21[131] = 63;
    v21[136] = 1065353217;
    v21[137] = 1065353218;
    v21[138] = 1065353220;
    v21[139] = 1065353224;
    v21[143] = 1065353216;
    v21[128] = 228;
    v21[130] = 0;
    v21[132] = 0;
    v21[140] = 1065353216;
    v21[141] = 1065353216;
    v21[142] = 1065353216;
    if ( (u32 *)v24 == v21 )
      TMAN_BSS_U32(0x5C) = 0;
  }
  if ( (*(u32 *)(v3 + 20) & 0x4000) != 0 )
  {
    *(u32 *)(*(u32 *)(v3 + 28) + 52) |= 0x40000000u;
    tman_thread_fn_7((u32 *)v3);
  }
  return InterruptManagerForKernel_551097F2(*(u32 *)(v3 + 28));
}


int sceKernelSuspendDispatchThread(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v5;

  tman_cfc0(13);
  if ( v2 != 0 )
    return -2147352476;
  v4 = sceKernelCpuSuspendIntr(a1, a2);
  if ( v4 != 0 )
  {
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((int (*)())TMAN_BSS_U32(0x48))(23, TMAN_BSS_U32(0x40), 0, v3);
    v5 = 0;
    if ( TMAN_BSS_U32(0x4C) == 0 )
    {
      TMAN_BSS_U32(0x4C) = 1;
      v5 = 1;
    }
    if ( *(int *)((&TMAN_BSS_U32(0x40))[0] + 20) < 0 )
      *(u32 *)(*(u32 *)((&TMAN_BSS_U32(0x40))[0] + 220) + 196) = 1;
    sceKernelCpuResumeIntr(v4);
    return v5;
  }
  else
  {
    sceKernelCpuResumeIntr(0);
    return -2147352474;
  }
}


int tman_thread_fn_10(int a1, int a2, int a3)
{
  u32 *v3;
  int v5;
  int v6;
  int v7;
  int v8;
  u32 *v9;

  v3 = (u32 *)(&TMAN_BSS_U32(0x40))[0];
  if ( a1 == 0 )
  {
    if ( TMAN_BSS_U32(0x4C) == 0 )
      TMAN_BSS_U32(0x4C) = 1;
    if ( *(int *)((&TMAN_BSS_U32(0x40))[0] + 20) < 0 )
      *(u32 *)(*(u32 *)((&TMAN_BSS_U32(0x40))[0] + 220) + 196) = 1;
    return 0;
  }
  v5 = TMAN_BSS_U32(0x4C);
  TMAN_BSS_U32(0x4C) = 0;
  if ( *(int *)((&TMAN_BSS_U32(0x40))[0] + 20) < 0 )
    *(u32 *)(*(u32 *)((&TMAN_BSS_U32(0x40))[0] + 220) + 196) = 0;
  switch ( v5 )
  {
    case 1:
      v6 = a3;
      v7 = 0;
LABEL_13:
      TMAN_BSS_U32(0x44) = 0;
      tman_sched_fn_19(v7, v6);
      return 0;
    case 2:
      v8 = TMAN_BSS_U32(0x4F4);
      v3[3] = 8;
      v6 = 1;
      v7 = 1;
      v3[1] = v8;
      TMAN_BSS_U32(0x4F4) = (int)v3;
      v9 = (u32 *)v3[1];
      *v3 = &TMAN_BSS_U32(0x4F0);
      *v9 = v3;
      goto LABEL_13;
    case 3:
      if ( a2 == 0 )
        tman_thread_fn_24(0, 0, -2147352148);
      tman_thread_fn_25(&v3[-*(u8 *)(TMAN_BSS_U32(0x50C) + 21)], 0, a3);
      return 0;
    default:
      return 0;
  }
}


int sceKernelResumeDispatchThread(int a1, int a2)
{
  int v2;
  int v4;
  int v5;

  tman_cfc0(13);
  v4 = -2147352476;
  if ( v2 == 0 )
  {
    v5 = sceKernelCpuSuspendIntr(a1, a2);
    if ( v5 != 0 )
    {
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((int (*)())TMAN_BSS_U32(0x48))(24, v5, 1, a1);
      v4 = tman_thread_fn_10(a1, 0, v5);
      sceKernelCpuResumeIntr(v5);
    }
    else
    {
      sceKernelCpuResumeIntr(0);
      return -2147352474;
    }
  }
  return v4;
}


int sceKernelChangeCurrentThreadAttr(int a1, int a2)
{
  u32 *v4;
  int v6;
  int v7;
  float v8;
  float v9;
  float v10;
  float v11;
  float v12;
  float v13;
  float v14;
  float v15;
  int v16;
  u32 v17;
  int v18;
  u32 *v19;
  u32 v20;
  u32 *v21;
  u32 *v22;
  int v23;
  u32 *v24;
  u32 v25;
  u32 v26[4];

  v4 = (u32 *)(&TMAN_BSS_U32(0x40))[0];
  tman_cfc0(13);
  if ( 0x20000 != 0 || *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 136) != 0 )
    return -2147352476;
  if ( (a1 & 0xFFFFBFFF) != 0 || (a2 & 0xFFFFBFFF) != 0 )
    return -2147352175;
  v6 = 25;
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((void (*)(int, int, int, int, int))TMAN_BSS_U32(0x48))(25, a1, 2, a1, a2);
  v7 = sceKernelCpuSuspendIntr(v6, a2);
  v16 = v4[5] & ~a1 | a2;
  v4[5] = v16;
  v17 = _mfc0(0xCu, 0);
  if ( (v16 & 0x4000) == 0 )
  {
    v18 = v4[5];
LABEL_22:
    if ( (v18 & 0x4000) == 0 && (v17 & 0x40000000) != 0 )
    {
      v24 = (u32 *)v4[9];
      *(u32 *)(v4[7] + 52) &= ~0x40000000u;
      if ( v24 != NULL )
        *v24 &= ~0x40000000u;
      v25 = _mfc0(0xCu, 0);
      _mtc0(0xCu, 0, v25 & 0xBFFFFFFF);
    }
LABEL_17:
    sceKernelCpuResumeIntr(v7);
    return 0;
  }
  if ( (v17 & 0x40000000) != 0 )
  {
    v18 = v4[5];
    goto LABEL_22;
  }
  if ( v4[8] != 0 )
  {
LABEL_14:
    v19 = (u32 *)v4[9];
    *(u32 *)(v4[7] + 52) |= 0x40000000u;
    if ( v19 != NULL )
      *v19 |= 0x40000000u;
    v20 = _mfc0(0xCu, 0);
    _mtc0(0xCu, 0, v20 | 0x40000000);
    tman_thread_fn_7(v4);
    goto LABEL_17;
  }
  v26[0] = 8;
  v26[1] = 64;
  v21 = (u32 *)sceKernelAllocHeapMemoryWithOption(TMAN_BSS_U32(0x820), 576, v26);
  v22 = v21;
  v4[8] = v21;
  v23 = 127;
  if ( v21 != NULL )
  {
    do
    {
      --v23;
      *v21++ = 2139095041;
    }
    while ( v23 >= 0 );
    v22[143] = 1065353216;
    v22[129] = 228;
    v22[131] = 63;
    v22[136] = 1065353217;
    v22[137] = 1065353218;
    v22[138] = 1065353220;
    v22[139] = 1065353224;
    v22[128] = 228;
    v22[130] = 0;
    v22[132] = 0;
    v22[140] = 1065353216;
    v22[141] = 1065353216;
    v22[142] = 1065353216;
    goto LABEL_14;
  }
  v4[5] &= ~0x4000u;
  sceKernelCpuResumeIntr(v7);
  return -2147352176;
}


int tman_thread_fn_11(u32 *a1, s32 a2, int a3)
{
  int v3;
  int v7;
  int v8;
  u32 *v9;
  int v11;
  int v12;
  int v13;
  u32 *v14;
  int v15;
  u32 *v16;
  int *v17;
  s32 v18;
  int v19;
  int *v20;
  int v21;
  int **v22;
  u32 v23;
  u32 v24;
  u32 v25;
  s32 v26;

  tman_cfc0(13);
  if ( a1 == (u32 *)(&TMAN_BSS_U32(0x40))[0] )
  {
    if ( TMAN_BSS_U32(0x4C) != 0 )
    {
      a1[4] = a2;
      return 0;
    }
    if ( v3 != 0 )
    {
      a1[4] = a2;
      a1[3] = 2;
      tman_thread_fn_5(a1);
      TMAN_BSS_U32(0x44) = 0;
      return 0;
    }
    v23 = _clz(TMAN_BSS_U32(0xD4));
    v24 = _clz(TMAN_BSS_U32(0xD0));
    if ( TMAN_BSS_U32(0xD0) == 0 )
      v24 = v23 + 32;
    v25 = _clz(TMAN_BSS_U32(0xCC));
    if ( TMAN_BSS_U32(0xCC) == 0 )
      v25 = v24 + 32;
    v26 = _clz(TMAN_BSS_U32(0xC8));
    if ( TMAN_BSS_U32(0xC8) == 0 )
      v26 = v25 + 32;
    if ( a2 < v26 )
    {
      a1[4] = a2;
      return 0;
    }
    a1[4] = a2;
    if ( a3 != 0 )
    {
      a1[3] = 2;
      tman_thread_fn_5(a1);
      v13 = a3;
    }
    else
    {
      v13 = 0;
    }
    TMAN_BSS_U32(0x44) = 0;
    goto LABEL_12;
  }
  v7 = a1[3];
  if ( v7 == 2 )
  {
    v14 = (u32 *)*a1;
    v15 = a1[1];
    if ( *a1 == v15 && v14 != a1 )
      (&TMAN_BSS_U32(0x40))[(a1[4] >> 5) + 34] &= ~__builtin_rotateright32(0x80000000, a1[4]);
    v14[1] = v15;
    v16 = (u32 *)a1[1];
    a1[4] = a2;
    *v16 = v14;
    if ( v3 != 0 || (v17 = (int *)(&TMAN_BSS_U32(0x40))[0], TMAN_BSS_U32(0x4C) != 0) )
    {
      tman_thread_fn_5(a1);
      TMAN_BSS_U32(0x44) = 0;
      return 0;
    }
    v18 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 16);
    if ( a2 >= v18 )
    {
      a1[3] = 2;
      tman_thread_fn_5(a1);
      return 0;
    }
    if ( a3 != 0 )
    {
      v19 = (&TMAN_BSS_U32(0x40))[2 * v18 + 38];
      v20 = &(&TMAN_BSS_U32(0x40))[(unsigned int)v18 >> 5];
      *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 4) = *(u32 *)(v19 + 4);
      *(u32 *)(v19 + 4) = v17;
      v21 = v20[34];
      v22 = (int **)v17[1];
      TMAN_BSS_U32(0x44) = (int)a1;
      v17[3] = 2;
      *v17 = v19;
      v20[34] = v21 | __builtin_rotateright32(0x80000000, v18);
      a1[3] = 1;
      *v22 = v17;
    }
    else
    {
      a1[3] = 2;
      tman_thread_fn_5(a1);
      TMAN_BSS_U32(0x44) = 0;
    }
    goto LABEL_11;
  }
  a1[4] = a2;
  if ( v7 == 4 || v7 == 0xC )
  {
    v8 = a1[23];
    if ( v8 != 0 && (*(u32 *)(v8 + 4 * *(u8 *)(TMAN_BSS_U32(0x508) + 21)) & 0x100) != 0 )
    {
      v9 = (u32 *)*a1;
      v9[1] = a1[1];
      *(u32 *)a1[1] = v9;
      tman_sched_fn_4(a1, v8);
      if ( tman_sched_fn_11(a1[23], v11, v12) != 0 && TMAN_BSS_U32(0x4C) == 0 )
      {
        TMAN_BSS_U32(0x44) = 0;
        if ( v3 == 0 )
        {
LABEL_11:
          v13 = a3;
LABEL_12:
          tman_sched_fn_19(0, v13);
        }
      }
    }
  }
  return 0;
}


int sceKernelChangeThreadPriority(int a1, s32 a2)
{
  int v2;
  int v3;
  s32 v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  s64 v11;
  int v12;
  s32 v13;
  int v15;
  u32 v16[4];

  tman_cfc0(13);
  v4 = a2;
  v5 = v3 << 11;
  v6 = a1;
  if ( a1 == 0 )
  {
    if ( v2 != 0 )
      return -2147352169;
    v6 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 8);
  }
  v7 = sceKernelCpuSuspendIntr(a1, a2);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v6, TMAN_BSS_U32(0x50C), v16) != 0 )
  {
    sceKernelCpuResumeIntr(v7);
    return -2147352168;
  }
  else
  {
    v8 = *(u16 *)(v16[0] + 22) & 2;
    if ( v5 >= 0 )
      v8 = 2;
    if ( v8 != 0 )
    {
      v9 = v16[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
      v10 = *(u32 *)(v9 + 12);
      v11 = ((u64)(v11) & 0xFFFFFFFF00000000ULL) | (u32)(v10 == 0x10);
      v11 = ((u64)(v11) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v10 == 0x20) << 32);
      if ( v11 != 0 )
      {
        sceKernelCpuResumeIntr(v7);
        return -2147352158;
      }
      else
      {
        if ( v4 == 0 )
          v4 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 16);
        v12 = (unsigned int)(v4 - 8) < 0x70;
        if ( *(int *)(v9 + 20) >= 0 )
          v12 = (unsigned int)(v4 - 1) < 0x7E;
        if ( v12 )
        {
          if ( TMAN_BSS_U32(0x48) != NULL )
            ((void (*)(int, s32, int, int, s32))TMAN_BSS_U32(0x48))(26, v4, 2, v6, v4);
          v13 = *(u32 *)(v9 + 188);
          *(u16 *)(v9 + 186) = v4;
          if ( v13 >= v4 )
          {
            v15 = tman_thread_fn_11((u32 *)v9, v4, v7);
            sceKernelCpuResumeIntr(v7);
            return v15;
          }
          else
          {
            sceKernelCpuResumeIntr(v7);
            return 0;
          }
        }
        else
        {
          sceKernelCpuResumeIntr(v7);
          return -2147352173;
        }
      }
    }
    else
    {
      sceKernelCpuResumeIntr(v7);
      return -2147352367;
    }
  }
}


int sceKernelRotateThreadReadyQueue(u32 a1)
{
  int v1;
  int v2;
  u32 v3;
  int v4;
  int v5;
  u32 *v6;
  u32 v7;
  u32 v8;
  u32 v9;
  int v10;
  u32 *v11;
  int *v12;
  int v13;
  u32 *v14;
  int **v15;

  v3 = a1;
  tman_cfc0(13);
  v4 = v1 != 0 && a1 == 0;
  v5 = v2 << 11;
  v6 = (u32 *)(&TMAN_BSS_U32(0x40))[0];
  if ( v4 != 0 )
  {
    v7 = _clz(TMAN_BSS_U32(0xD4));
    v8 = _clz(TMAN_BSS_U32(0xD0));
    v4 = TMAN_BSS_U32(0xC8);
    if ( TMAN_BSS_U32(0xD0) == 0 )
      v8 = v7 + 32;
    v9 = _clz(TMAN_BSS_U32(0xCC));
    if ( TMAN_BSS_U32(0xCC) == 0 )
      v9 = v8 + 32;
    v3 = _clz(TMAN_BSS_U32(0xC8));
    if ( TMAN_BSS_U32(0xC8) == 0 )
      v3 = v9 + 32;
  }
  if ( v5 >= 0 )
  {
    if ( v3 < 0x7F )
      goto LABEL_10;
    return -2147352173;
  }
  if ( v3 != 0 && v3 - 8 >= 0x70 )
    return -2147352173;
LABEL_10:
  v10 = sceKernelCpuSuspendIntr(v4, v3 < 0x7F);
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(27, v3, 1, v3);
  if ( (v1 | v3) == 0 )
    v3 = v6[4];
  v11 = (u32 *)((char *)&TMAN_BSS_U8(0xD8) + 8 * v3);
  if ( (u32 *)*v11 != v11 )
  {
    if ( v3 == v6[4] )
    {
      if ( TMAN_BSS_U32(0x4C) == 0 )
      {
        if ( v1 != 0 )
        {
          v6[3] = 2;
          tman_thread_fn_5(v6);
          TMAN_BSS_U32(0x44) = 0;
        }
        else
        {
          if ( v10 != 0 )
          {
            v6[3] = 2;
            tman_thread_fn_5(v6);
          }
          TMAN_BSS_U32(0x44) = 0;
          tman_sched_fn_19(0, v10);
        }
      }
    }
    else
    {
      v12 = (int *)(&TMAN_BSS_U32(0x40))[2 * v3 + 38];
      v13 = *v12;
      *(u32 *)(v13 + 4) = v12[1];
      v14 = (u32 *)v12[1];
      v12[1] = v11[1];
      v11[1] = v12;
      v15 = (int **)v12[1];
      *v14 = v13;
      *v12 = (int)v11;
      *v15 = v12;
    }
  }
  sceKernelCpuResumeIntr(v10);
  return 0;
}


int sceKernelReleaseWaitThread(int a1, int a2)
{
  int v2;
  int v3;
  int v5;
  int v6;
  int v7;
  int v8;
  int *v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  u32 v18[4];

  v5 = v3 << 11;
  if ( a1 == 0 )
    return -2147352169;
  v6 = sceKernelCpuSuspendIntr(a1, a2);
  tman_cfc0(13);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x50C), v18) != 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352168;
  }
  v7 = *(u16 *)(v18[0] + 22) & 2;
  if ( v5 >= 0 )
    v7 = 2;
  if ( v7 == 0 )
  {
    v8 = v6;
LABEL_22:
    sceKernelCpuResumeIntr(v8);
    return -2147352367;
  }
  v9 = (int *)(v18[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21));
  if ( v2 == 0 && v9 == (int *)(&TMAN_BSS_U32(0x40))[0] )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352169;
  }
  if ( v9[3] != 4 && v9[3] != 0xC )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352154;
  }
  else
  {
    v10 = v9[23];
    if ( v10 != 0 )
    {
      v11 = *(u16 *)(v10 + 22) & 8;
      if ( v5 >= 0 )
        v11 = 8;
      v8 = v6;
      if ( v11 == 0 )
        goto LABEL_22;
    }
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((int (*)())TMAN_BSS_U32(0x48))(28, a1, 1, a1);
    v12 = tman_sched_fn_6(v9);
    v15 = v9[23];
    v16 = v12;
    v9[32] = -2147352150;
    if ( v15 != 0 )
      v16 = v12 + tman_sched_fn_11(v15, v13, v14);
    if ( v16 != 0 )
    {
      TMAN_BSS_U32(0x44) = 0;
      if ( v2 == 0 )
        tman_sched_fn_19(0, v6);
    }
    sceKernelCpuResumeIntr(v6);
    return 0;
  }
}


int sceKernelGetThreadExitStatus(int a1, int a2)
{
  int v2;
  int v3;
  int v5;
  int v6;
  int v7;
  u32 v9[4];

  v5 = sceKernelCpuSuspendIntr(a1, a2);
  v3 = v2 << 11;
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(a1, TMAN_BSS_U32(0x50C), v9) != 0 )
  {
    sceKernelCpuResumeIntr(v5);
    return -2147352168;
  }
  else
  {
    v6 = *(u16 *)(v9[0] + 22) & 1;
    if ( v3 >= 0 )
      v6 = 1;
    if ( v6 != 0 )
    {
      v7 = *(u32 *)(v9[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21) + 84);
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((int (*)())TMAN_BSS_U32(0x48))(31, TMAN_BSS_U32(0x40), 2, a1);
      sceKernelCpuResumeIntr(v5);
      return v7;
    }
    else
    {
      sceKernelCpuResumeIntr(v5);
      return -2147352367;
    }
  }
}


int sceKernelReturnFromChangeStack(int a1, int a2)
{
  u32 *v2;
  int v3;
  int v4;
  int *v5;
  u32 *v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;

  v2 = (u32 *)(&TMAN_BSS_U32(0x40))[0];
  tman_cfc0(13);
  if ( 0x20000 != 0 || (v3 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 212)) == 0 )
    _break(3u, 0x3FFu);
  v4 = sceKernelCpuSuspendIntr(v3, a2);
  v5 = (int *)v2[53];
  v6 = (u32 *)v2[9];
  v7 = v2[49];
  v8 = v2[76];
  v9 = v6[6];
  v6[1] = v5[4];
  v10 = *v5;
  v11 = v5[1];
  v6[3] = v5[5];
  v12 = v5[6];
  v13 = v2[11];
  v2[53] = v10;
  v6[2] = v12;
  v14 = v5[2];
  v2[11] = v11;
  v2[76] = v14;
  v2[49] = v5[3];
  sceKernelCpuResumeIntr(v4);
  if ( (v2[5] & 0x200000) != 0 )
    sceKernelMemset32(v13, 0, v7);
  if ( v8 != 0 )
    sceKernelFreePartitionMemory(v8);
  return v9;
}


int ThreadManForUser_28BFD974(int a1, int a2, int a3)
{
  int v3;
  int v4;
  int v5;
  int result;
  int v10[10];
  int v11;
  unsigned int v12;

  v4 = v3;
  v5 = v3 << 11;
  v10[0] = 56;
  result = sceKernelQueryMemoryBlockInfo(a1, v10);
  if ( result >= 0 )
  {
    if ( (v5 & ((v11 + v12) | v11 | v12) & 0x80000000) != 0 )
    {
      return -2147352365;
    }
    else if ( v12 < 0x200 )
    {
      return -2147352172;
    }
    else
    {
      result = tman_thread_fn_26(0, v11, v12, a2, a3);
      if ( result >= 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((int (*)())TMAN_BSS_U32(0x48))(40, v4, 3, a1);
        return -559038737;
      }
    }
  }
  return result;
}


int sceKernelExtendThreadStack(unsigned int a1, int a2, int a3)
{
  unsigned int v3;
  int v4;
  int v7;
  int v8;
  int v9;
  int BlockHeadAddr;
  int v11;

  v3 = (a1 + 255) & 0xFFFFFF00;
  v4 = -2147352172;
  v7 = (&TMAN_BSS_U32(0x40))[0];
  if ( a1 >= 0x200 )
  {
    v8 = sceKernelCpuSuspendIntr(0, a2);
    snprintf(&TMAN_BSS_U32(0x714), 31, "exstack:%s", *(const char **)(v7 - 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21) + 16));
    v9 = sceKernelAllocPartitionMemory(
           *(u32 *)(v7 + 312),
           &TMAN_BSS_U32(0x714),
           ((u8)(*(u32 *)(v7 + 20) >> 22) ^ 1) & 1,
           v3,
           0);
    sceKernelCpuResumeIntr(v8);
    v4 = -2147352176;
    if ( v9 > 0 )
    {
      BlockHeadAddr = sceKernelGetBlockHeadAddr(v9);
      v11 = tman_thread_fn_26(v9, BlockHeadAddr, v3, a2, a3);
      if ( v11 >= 0 )
      {
        if ( TMAN_BSS_U32(0x48) != NULL )
          ((void (*)(int, int, int, unsigned int, int, int))TMAN_BSS_U32(0x48))(41, v9, 3, v3, a2, a3);
        return -559038737;
      }
      else
      {
        sceKernelFreePartitionMemory(v9);
        return v11;
      }
    }
  }
  return v4;
}


int sceKernelExtendKernelStack(unsigned int a1, int (*a2)(int), int a3)
{
  int v3;
  int v7;
  u32 *v8;
  int v9;
  unsigned int v10;
  int BlockHeadAddr;
  int v12;
  int v13;
  int v14;
  u32 *v15;
  void (*v16)(int, u32 *, int, int (*)(int), int);
  int v17;
  int v18;
  int v19;
  int v20;
  int v22;
  u32 InterruptMask;
  u32 *v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  int v35;
  int v36;
  int v37;
  int v38;
  int v39;

  v7 = v3 << 11;
  v8 = (u32 *)(&TMAN_BSS_U32(0x40))[0];
  tman_cfc0(13);
  if ( 0x20000 != 0 )
    return -2147352476;
  if ( v7 < 0 )
    return -2147352367;
  if ( (int)a2 >= 0 )
    return -2147352365;
  if ( a1 < 0x200 )
    return -2147352172;
  v9 = sceKernelCpuSuspendIntr(a1, a2);
  snprintf(&TMAN_BSS_U32(0x714), 31, "exkstack:%s", (const char *)v8[-*(u8 *)(TMAN_BSS_U32(0x50C) + 21) + 4]);
  v10 = (a1 + 255) & 0xFFFFFF00;
  v39 = sceKernelAllocPartitionMemory(v8[79], &TMAN_BSS_U32(0x714), 1, v10, 0);
  sceKernelCpuResumeIntr(v9);
  if ( v39 <= 0 )
    return -2147352176;
  BlockHeadAddr = sceKernelGetBlockHeadAddr(v39);
  v14 = v8[5];
  v15 = (u32 *)BlockHeadAddr;
  if ( v14 >= 0 )
  {
    v12 = BlockHeadAddr;
    if ( (v14 & 0x100000) == 0 )
      sceKernelMemset32(BlockHeadAddr, -1, v10);
  }
  v16 = (void (*)(int, u32 *, int, int (*)(int), int))TMAN_BSS_U32(0x48);
  *v15 = v8[2];
  if ( v16 != NULL )
    v16(42, v8, 2, a2, a3);
  sceKernelCpuSuspendIntr(v12, v13);
  v17 = v8[5];
  v35 = v8[52];
  v8[52] = &v35;
  if ( v17 < 0 )
  {
    v31 = v8[12];
    v32 = v39;
    v37 = v8[77];
    v33 = v8[51];
    v36 = v31;
    v38 = v33;
    v8[12] = v15;
    v8[51] = v10;
    v8[77] = v32;
  }
  else
  {
    v18 = v8[11];
    v19 = v39;
    v37 = v8[76];
    v20 = v8[49];
    v36 = v18;
    v38 = v20;
    v8[11] = v15;
    v8[76] = v19;
    v8[49] = v10;
  }
  tman_mtic_0(1);
  v22 = a2(a3);
  InterruptMask = getInterruptMask();
  tman_mtic_0(0);
  v25 = (u32 *)v8[52];
  if ( (int)v8[5] < 0 )
  {
    v29 = v25[1];
    v8[77] = v25[2];
    v30 = v25[3];
    v8[12] = v29;
    v8[51] = v30;
  }
  else
  {
    v26 = v25[1];
    v8[76] = v25[2];
    v27 = v25[3];
    v8[11] = v26;
    v8[49] = v27;
  }
  v8[52] = *v25;
  sceKernelCpuResumeIntr(InterruptMask);
  v28 = v8[5];
  if ( v28 >= 0 && (v28 & 0x200000) != 0 )
    sceKernelMemset32(v15, 0, v10);
  sceKernelFreePartitionMemory(v39);
  return v22;
}


void tman_thread_fn_12(u32 *a1, int a2)
{
  int v3;
  int v5;
  int v6;
  int v7;
  int v8;
  u32 *v9;
  u32 *v10;
  int j;
  int v12;
  int i;

  v3 = a1[5];
  if ( v3 < 0 && a1[55] != 0 )
  {
    tman_mem_fn_22((int)a1, 0x200000);
    v5 = a1[55];
    v6 = v5 - 256;
    do
    {
      v6 += 4;
      *(u32 *)(v6 + 252) = 0;
    }
    while ( v6 != v5 );
    v3 = a1[5];
  }
  if ( (v3 & 0x200000) != 0 )
  {
    sceKernelCpuResumeIntr(a2);
    sceKernelMemset32(a1[11], 0, a1[49]);
    if ( (int)a1[5] >= 0 )
    {
      v10 = (u32 *)a1[52];
    }
    else
    {
      v9 = (u32 *)a1[53];
      if ( v9 != NULL )
      {
        for ( i = v9[1]; ; i = v9[1] )
        {
          sceKernelMemset32(i, 0, v9[3]);
          v9 = (u32 *)*v9;
          if ( v9 == NULL )
            break;
        }
        v12 = a1[12];
      }
      else
      {
        v12 = a1[12];
      }
      sceKernelMemset32(v12, 0, a1[51]);
      v10 = (u32 *)a1[52];
    }
    if ( v10 != NULL )
    {
      for ( j = v10[1]; ; j = v10[1] )
      {
        sceKernelMemset32(j, 0, v10[3]);
        v10 = (u32 *)*v10;
        if ( v10 == NULL )
          break;
      }
    }
    sceKernelCpuSuspendIntr(v7, v8);
  }
}


int tman_thread_fn_13(int *a1, int a2, int *a3)
{
  int *v3;
  int v5;
  int v6;
  int result;
  int *i;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int v15;

  v3 = (int *)a1[52];
  if ( v3 != NULL )
  {
    do
    {
      if ( a1[5] >= 0 )
      {
        v13 = a1[76];
        v5 = v3[1];
        a1[76] = v3[2];
        v6 = v3[3];
        a1[11] = v5;
        a1[49] = v6;
      }
      else
      {
        v13 = a1[77];
        v14 = v3[1];
        a1[77] = v3[2];
        v15 = v3[3];
        a1[12] = v14;
        a1[51] = v15;
      }
      a1[52] = *v3;
      result = sceKernelFreePartitionMemory(v13);
      a3 = (int *)a1[52];
      v3 = a3;
    }
    while ( a3 != NULL );
  }
  if ( a1[5] < 0 )
  {
    for ( i = (int *)a1[53]; i != NULL; i = (int *)a1[53] )
    {
      v9 = a1[76];
      v10 = i[1];
      a1[49] = i[3];
      v11 = i[2];
      v12 = *i;
      a1[11] = v10;
      a1[76] = v11;
      a1[53] = v12;
      result = sceKernelFreeHeapMemory(TMAN_BSS_U32(0x820), i, a3);
      if ( v9 != 0 )
        result = sceKernelFreePartitionMemory(v9);
    }
  }
  return result;
}


int sceKernelCheckThreadStack(int a1, char *a2)
{
  u32 *v2;
  int result;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  u32 v11[4];

  tman_cfc0(13);
  v2 = (u32 *)(&TMAN_BSS_U32(0x40))[0];
  if ( a1 != 0 )
    return 0;
  v4 = 32;
  if ( TMAN_BSS_U32(0x48) != NULL )
  {
    ((void (*)(int, unsigned int, u32))TMAN_BSS_U32(0x48))(32, 0x20000u, 0);
    v5 = v2[5];
  }
  else
  {
    v5 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 20);
  }
  v6 = (&TMAN_BSS_U32(0x40))[0];
  if ( v5 < 0 )
  {
    v9 = *(u32 *)(v2[9] + 8);
    v10 = v9 - v2[11];
    if ( v10 < 64 )
    {
      sceKernelCpuSuspendIntr(v4, a2);
      SaveThreadContext(v2[7]);
      tman_thread_fn_22(v2, 0, v9);
      tman_sched_fn_19(1, 1);
    }
    return v10;
  }
  else
  {
    v7 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 20);
    if ( v7 < 0 )
    {
      a2 = (char *)v11 - *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 48);
      v11[0] = a2;
    }
    else
    {
      v8 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 44);
      result = 0;
      if ( v8 == 0 )
        return result;
      v11[0] = (char *)v11 - v8;
    }
    if ( v11[0] < 64 )
    {
      sceKernelCpuSuspendIntr(v7, a2);
      SaveThreadContext(*(u32 *)(v6 + 28));
      tman_thread_fn_22(v6, *(u32 *)(v6 + 20) >> 31, *(u32 *)(*(u32 *)(v6 + 28) + 180));
      tman_sched_fn_19(1, 1);
    }
    return v11[0];
  }
}


int sceKernelGetThreadStackFreeSize(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  unsigned int v10;
  int v11;
  unsigned int v12;
  int v13;
  unsigned int v14;
  int i;
  int v16;
  u32 v18[4];

  tman_cfc0(13);
  v4 = v3 << 11;
  v5 = a1;
  if ( v2 != 0 )
    return -2147352476;
  v6 = sceKernelCpuSuspendIntr(a1, a2);
  if ( v5 == 0 )
    v5 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 8);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v5, TMAN_BSS_U32(0x50C), v18) != 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352168;
  }
  else
  {
    v7 = *(u16 *)(v18[0] + 22) & 1;
    if ( v4 >= 0 )
      v7 = 1;
    if ( v7 != 0 )
    {
      v8 = *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
      v9 = v18[0] + 4 * v8;
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((int (*)())TMAN_BSS_U32(0x48))(33, v18[0] + 4 * v8, 1, v5);
      v10 = *(u32 *)(v9 + 196);
      v11 = *(u32 *)(v9 + 44);
      sceKernelCpuResumeIntr(v6);
      v12 = (v10 >> 2) - 4;
      v13 = v11 + 16;
      v14 = 0;
      for ( i = 0; v14 < v12; i = 4 * v14 )
      {
        i = 4 * v14;
        v16 = *(u32 *)(4 * v14++ + v13);
        if ( v16 != -1 )
          break;
      }
      return i;
    }
    else
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352367;
    }
  }
}


int sceKernelGetThreadKernelStackFreeSize(int a1, int a2)
{
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int *v8;
  int v9;
  int v10;
  unsigned int v11;
  int v12;
  int v13;
  unsigned int v15;
  int v16[4];

  tman_cfc0(13);
  v4 = a1;
  v5 = v3 << 11;
  if ( v2 != 0 )
    return -2147352476;
  v6 = sceKernelCpuSuspendIntr(a1, a2);
  if ( v4 == 0 )
    v4 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 8);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v4, TMAN_BSS_U32(0x50C), v16) != 0 )
  {
    sceKernelCpuResumeIntr(v6);
    return -2147352168;
  }
  else
  {
    v7 = *(u16 *)(v16[0] + 22) & 1;
    if ( v5 >= 0 )
      v7 = 1;
    if ( v7 != 0 )
    {
      v8 = (int *)(v16[0] + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21));
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((int (*)())TMAN_BSS_U32(0x48))(44, v4, 1, v4);
      if ( v8[5] >= 0 )
      {
        v9 = v8[11];
        v15 = v8[49];
      }
      else
      {
        v9 = v8[12];
        v15 = v8[51];
      }
      sceKernelCpuResumeIntr(v6);
      v10 = v9 + 16;
      v11 = 0;
      v12 = 0;
      if ( v15 >> 2 != 4 )
      {
        do
        {
          v12 = 4 * v11;
          v13 = *(u32 *)(4 * v11++ + v10);
          if ( v13 != -1 )
            break;
          v12 = 4 * v11;
        }
        while ( v11 < (v15 >> 2) - 4 );
      }
      return v12;
    }
    else
    {
      sceKernelCpuResumeIntr(v6);
      return -2147352367;
    }
  }
}


int sceKernelReferThreadStatus(int a1, unsigned int *a2)
{
  int v2;
  int v4;
  int v5;
  int v6;
  int v7;
  int UIDcontrolBlockWithType;
  int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  int *v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  int v24;
  s64 v25;
  unsigned int v26;
  int v27;
  s64 v29;
  u32 v30[4];

  v4 = (unsigned int)(a2 + 27) | (unsigned int)a2;
  v5 = v4 & (v2 << 11);
  tman_cfc0(13);
  v6 = v2 << 11;
  v7 = a1;
  if ( v5 < 0 )
    return -2147352365;
  if ( a1 == 0 )
  {
    if ( v4 != 0 )
      return -2147352169;
    v7 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 8);
  }
  v9 = sceKernelCpuSuspendIntr(a1, v5);
  UIDcontrolBlockWithType = sceKernelGetUIDcontrolBlockWithType(v7, TMAN_BSS_U32(0x50C), v30);
  v10 = v30[0];
  if ( UIDcontrolBlockWithType != 0 )
  {
    sceKernelCpuResumeIntr(v9);
    return -2147352168;
  }
  else
  {
    v11 = *(u16 *)(v30[0] + 22) & 1;
    if ( v6 >= 0 )
      v11 = 1;
    if ( v11 != 0 )
    {
      v12 = *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
      v13 = v30[0] + 4 * v12;
      if ( TMAN_BSS_U32(0x48) != NULL )
      {
        ((void (*)(int, int, int, int, unsigned int *))TMAN_BSS_U32(0x48))(34, v30[0] + 4 * v12, 2, v7, a2);
        v10 = v30[0];
      }
      v14 = *(u32 *)(v10 + 16);
      v15 = (int *)&TMAN_BSS_U8(0x6A8);
      do
      {
        ++v15;
        v15[26] = 0;
      }
      while ( v15 != &TMAN_BSS_U32(0x714) );
      TMAN_BSS_U32(0x714) = 108;
      if ( v14 != 0 )
        strncpy(&TMAN_BSS_U32(0x718), v14, 31);
      v16 = *(u32 *)(v13 + 20);
      v17 = *(s16 *)(v13 + 184);
      v18 = *(u32 *)(v13 + 192);
      TMAN_BSS_U32(0x738) = v16;
      v19 = *(u32 *)(v13 + 12);
      v20 = *(u32 *)(v13 + 44);
      TMAN_BSS_U32(0x740) = v18;
      TMAN_BSS_U32(0x73C) = v19;
      v21 = *(u32 *)(v13 + 196);
      TMAN_BSS_U32(0x750) = v17;
      TMAN_BSS_U8(0x748) = v21;
      v22 = *(u32 *)(v13 + 16);
      TMAN_BSS_U8(0x744) = v20;
      TMAN_BSS_U32(0x754) = v22;
      TMAN_BSS_U32(0x74C) = *(u32 *)(v13 + 216);
      if ( v19 == 4 || v19 == 0xC )
      {
        if ( *(u32 *)(v13 + 152) != 0 )
          v23 = *(u32 *)(v13 + 88) + 256;
        else
          v23 = *(u32 *)(v13 + 88);
        TMAN_BSS_U32(0x758) = v23;
        v24 = *(u32 *)(v13 + 92);
        if ( v24 != 0 )
          TMAN_BSS_U32(0x75C) = *(u32 *)(v24 + 12) ^ TMAN_BSS_U32(0x844);
        else
          TMAN_BSS_U32(0x75C) = 0;
      }
      TMAN_BSS_U32(0x760) = *(u32 *)(v13 + 80);
      TMAN_BSS_U32(0x764) = *(u32 *)(v13 + 84);
      v25 = *(u64 *)(v13 + 52);
      if ( (&TMAN_BSS_U32(0x40))[0] == v13 )
      {
        v29 = ((u64)(v29) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
        v16 = v29 - TMAN_BSS_U64(0xB0);
        v25 += v29 - TMAN_BSS_U64(0xB0);
      }
      TMAN_BSS_U64(0x768) = v25;
      TMAN_BSS_U32(0x770) = *(u32 *)(v13 + 68);
      TMAN_BSS_U32(0x774) = *(u32 *)(v13 + 72);
      TMAN_BSS_U32(0x778) = *(u32 *)(v13 + 76);
      TMAN_BSS_U32(0x77C) = *(u32 *)(v13 + 156);
      if ( sceKernelGetCompiledSdkVersion(v16) <= 33947664 )
      {
        TMAN_BSS_U32(0x714) = 104;
        v26 = *a2;
        TMAN_BSS_U32(0x758) &= 0xFu;
      }
      else
      {
        v26 = *a2;
        if ( v26 >= 0x6D )
        {
          sceKernelCpuResumeIntr(v9);
          return -2147352132;
        }
      }
      v27 = TMAN_BSS_U32(0x714);
      if ( TMAN_BSS_U32(0x714) >= v26 )
        v27 = v26;
      memcpy(a2, &TMAN_BSS_U32(0x714), v27);
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


int sceKernelReferThreadRunStatus(int a1, unsigned int *a2)
{
  int v2;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int *v12;
  s64 v13;
  int v14;
  s64 v15;
  unsigned int v16;
  s64 v18;
  u32 v19[4];

  v4 = (unsigned int)(a2 + 12) | (unsigned int)a2;
  v5 = v4 & (v2 << 11);
  v6 = v2 << 11;
  v7 = a1;
  if ( v5 < 0 )
    return -2147352365;
  tman_cfc0(13);
  if ( a1 != 0 )
    return -2147352476;
  v8 = sceKernelCpuSuspendIntr(0, v4);
  if ( v7 == 0 )
    v7 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 8);
  if ( (unsigned int)sceKernelGetUIDcontrolBlockWithType(v7, TMAN_BSS_U32(0x50C), v19) != 0 )
  {
    sceKernelCpuResumeIntr(v8);
    return -2147352168;
  }
  else
  {
    v9 = *(u16 *)(v19[0] + 22) & 1;
    if ( v6 >= 0 )
      v9 = 1;
    if ( v9 != 0 )
    {
      v10 = *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
      v11 = v19[0] + 4 * v10;
      if ( TMAN_BSS_U32(0x48) != NULL )
        ((void (*)(int, int, int, int, unsigned int *))TMAN_BSS_U32(0x48))(35, v19[0] + 4 * v10, 2, v7, a2);
      v12 = (int *)&TMAN_BSS_U8(0x6E4);
      do
      {
        ++v12;
        v12[11] = 0;
      }
      while ( v12 != &TMAN_BSS_U32(0x714) );
      TMAN_BSS_U32(0x714) = 48;
      TMAN_BSS_U32(0x718) = *(u32 *)(v11 + 12);
      v13 = ((u64)(v13) & 0xFFFFFFFF00000000ULL) | (u32)(TMAN_BSS_U32(0x718) == 4);
      v13 = ((u64)(v13) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(TMAN_BSS_U32(0x718) == 0xC) << 32);
      TMAN_BSS_U32(0x71C) = *(u32 *)(v11 + 16);
      if ( v13 != 0 )
      {
        v14 = *(u32 *)(v11 + 92);
        TMAN_BSS_U32(0x720) = *(u32 *)(v11 + 88);
        if ( v14 != 0 )
          TMAN_BSS_U32(0x724) = *(u32 *)(v14 + 12) ^ TMAN_BSS_U32(0x844);
        else
          TMAN_BSS_U32(0x724) = 0;
      }
      TMAN_BSS_U32(0x728) = *(u32 *)(v11 + 80);
      v15 = *(u64 *)(v11 + 52);
      if ( (&TMAN_BSS_U32(0x40))[0] == v11 )
      {
        v18 = ((u64)(v18) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
        v15 += v18 - TMAN_BSS_U64(0xB0);
      }
      TMAN_BSS_U8(0x72C) = v15;
      v16 = *a2;
      TMAN_BSS_U32(0x734) = *(u32 *)(v11 + 68);
      if ( TMAN_BSS_U32(0x714) < v16 )
        v16 = TMAN_BSS_U32(0x714);
      TMAN_BSS_U32(0x738) = *(u32 *)(v11 + 72);
      TMAN_BSS_U32(0x73C) = *(u32 *)(v11 + 76);
      TMAN_BSS_U32(0x740) = *(u32 *)(v11 + 156);
      memcpy(a2, &TMAN_BSS_U32(0x714), v16);
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


int sceKernelGetThreadmanIdList(int a1, int *a2, int a3, unsigned int a4)
{
  int v4;
  int *v5;
  int v6;
  int v8;
  int v10;
  int v12;
  int v14;
  int v15;
  int *v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int *v21;
  int *v22;
  int v23;
  int *v24;
  int v25;
  int IsKindOf;
  int v27;
  int *v28;
  int v29;
  int i;
  int v31;
  int *v32;

  v5 = a2;
  v6 = v4 << 11;
  v8 = (v4 << 11) & ((unsigned int)&a2[a3] | (unsigned int)a2 | (4 * a3));
  v10 = v4 << 11;
  if ( v8 < 0 )
    return -2147352365;
  v12 = ((a4 + 4) | a4) & v6;
  if ( v12 < 0 )
    return -2147352365;
  v14 = sceKernelCpuSuspendIntr(v12, v8);
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((void (*)(int, int, int, int, int *, int, unsigned int))TMAN_BSS_U32(0x48))(165, a1, 4, a1, v5, a3, a4);
  if ( (unsigned int)(a1 - 1) < 0x3F )
  {
    switch ( a1 )
    {
      case 1:
        v15 = TMAN_BSS_U32(0x50C);
        break;
      case 2:
        v15 = TMAN_BSS_U32(0x510);
        break;
      case 3:
        v15 = TMAN_BSS_U32(0x514);
        break;
      case 4:
        v15 = TMAN_BSS_U32(0x520);
        break;
      case 5:
        v15 = TMAN_BSS_U32(0x528);
        break;
      case 6:
        v15 = TMAN_BSS_U32(0x52C);
        break;
      case 7:
        v15 = TMAN_BSS_U32(0x524);
        break;
      case 8:
        v15 = TMAN_BSS_U32(0x538);
        break;
      case 9:
        v15 = TMAN_BSS_U32(0x534);
        break;
      case 10:
        v15 = TMAN_BSS_U32(0x544);
        break;
      case 11:
        v15 = TMAN_BSS_U32(0x548);
        break;
      case 12:
        v15 = TMAN_BSS_U32(0x518);
        break;
      case 13:
        v15 = TMAN_BSS_U32(0x51C);
        break;
      case 14:
        v15 = TMAN_BSS_U32(0x530);
        break;
      default:
        goto LABEL_28;
    }
    v16 = *(int **)v15;
    v17 = 0;
    v18 = 0;
    v32 = *(int **)v15;
    if ( v32 != (int *)v15 )
    {
      v19 = TMAN_BSS_U32(0x50C);
      while ( v15 == v19 )
      {
        v22 = &v16[*(u8 *)(v15 + 21)];
        if ( v22 == (int *)TMAN_BSS_U32(0x54C) )
        {
          v21 = (int *)*v16;
        }
        else
        {
          if ( v22 != (int *)TMAN_BSS_U32(0x550) )
          {
            v20 = *((u8 *)v16 + 22);
            goto LABEL_13;
          }
          v21 = (int *)*v16;
        }
LABEL_17:
        v32 = v21;
        v16 = v21;
        if ( v21 == (int *)v15 )
          goto LABEL_18;
      }
      v20 = *((u8 *)v16 + 22);
LABEL_13:
      if ( v10 >= 0 )
        v20 = 255;
      if ( v20 != 0 )
      {
        ++v18;
        if ( v17 < a3 )
        {
          ++v17;
          *v5++ = v16[3] ^ TMAN_BSS_U32(0x844);
        }
      }
      v21 = (int *)*v16;
      goto LABEL_17;
    }
    goto LABEL_18;
  }
  if ( a1 == 66 )
  {
    v24 = &TMAN_BSS_U32(0x4F0);
  }
  else if ( a1 >= 67 )
  {
    v25 = a1;
    if ( a1 != 67 )
    {
LABEL_48:
      if ( sceKernelGetUIDcontrolBlock(v25, &v32) != 0 )
      {
LABEL_28:
        v23 = v14;
      }
      else
      {
        IsKindOf = sceKernelIsKindOf(v32, TMAN_BSS_U32(0x508));
        v23 = v14;
        if ( IsKindOf != 0 )
        {
          v27 = *((u16 *)v32 + 11) & 1;
          if ( v10 >= 0 )
            v27 = 1;
          if ( v27 == 0 )
          {
            sceKernelCpuResumeIntr(v14);
            return -2147352367;
          }
          v24 = &v32[*(u8 *)(TMAN_BSS_U32(0x508) + 21) + 3];
          goto LABEL_55;
        }
      }
      sceKernelCpuResumeIntr(v23);
      return -2147352133;
    }
    v24 = &TMAN_BSS_U32(0x4E8);
  }
  else
  {
    v24 = &TMAN_BSS_U32(0x4D8);
    if ( a1 != 64 )
    {
      v24 = &TMAN_BSS_U32(0x4E0);
      if ( a1 != 65 )
      {
        v25 = a1;
        goto LABEL_48;
      }
    }
  }
LABEL_55:
  v28 = (int *)*v24;
  v17 = 0;
  v18 = 0;
  if ( (int *)*v24 != v24 )
  {
    v29 = TMAN_BSS_U32(0x50C);
    for ( i = *(u8 *)(TMAN_BSS_U32(0x50C) + 21); ; i = *(u8 *)(v29 + 21) )
    {
      v31 = BYTE2(v28[-i + 5]);
      if ( v10 >= 0 )
        v31 = 255;
      if ( v31 != 0 )
      {
        ++v18;
        if ( v17 < a3 )
        {
          ++v17;
          *v5++ = v28[2];
        }
      }
      v28 = (int *)*v28;
      if ( v28 == v24 )
        break;
    }
  }
LABEL_18:
  sceKernelCpuResumeIntr(v14);
  if ( a4 != 0 )
    *(u32 *)a4 = v18;
  return v17;
}


int sceKernelGetThreadmanIdType(int a1, int a2)
{
  int v2;
  int v4;
  int UIDcontrolBlock;
  int v6;
  int v7;
  int v8;
  int v9;
  u32 v11[4];

  v4 = v2 << 11;
  v6 = sceKernelCpuSuspendIntr(a1, a2);
  UIDcontrolBlock = sceKernelGetUIDcontrolBlock(a1, v11);
  v7 = v11[0];
  if ( UIDcontrolBlock != 0 )
    goto LABEL_26;
  v8 = *(u8 *)(v11[0] + 22);
  if ( v4 >= 0 )
    v8 = 255;
  if ( v8 == 0 )
  {
LABEL_26:
    sceKernelCpuResumeIntr(v6);
    return -2147352366;
  }
  else
  {
    if ( TMAN_BSS_U32(0x48) != NULL )
    {
      ((int (*)())TMAN_BSS_U32(0x48))(166, a1, 1, a1);
      v7 = v11[0];
    }
    v9 = 1;
    if ( sceKernelIsKindOf(v7, TMAN_BSS_U32(0x50C)) == 0 )
    {
      v9 = 2;
      if ( sceKernelIsKindOf(v11[0], TMAN_BSS_U32(0x510)) == 0 )
      {
        v9 = 3;
        if ( sceKernelIsKindOf(v11[0], TMAN_BSS_U32(0x514)) == 0 )
        {
          v9 = 4;
          if ( sceKernelIsKindOf(v11[0], TMAN_BSS_U32(0x520)) == 0 )
          {
            v9 = 5;
            if ( sceKernelIsKindOf(v11[0], TMAN_BSS_U32(0x528)) == 0 )
            {
              v9 = 6;
              if ( sceKernelIsKindOf(v11[0], TMAN_BSS_U32(0x52C)) == 0 )
              {
                v9 = 7;
                if ( sceKernelIsKindOf(v11[0], TMAN_BSS_U32(0x524)) == 0 )
                {
                  v9 = 8;
                  if ( sceKernelIsKindOf(v11[0], TMAN_BSS_U32(0x538)) == 0 )
                  {
                    v9 = 9;
                    if ( sceKernelIsKindOf(v11[0], TMAN_BSS_U32(0x534)) == 0 )
                    {
                      v9 = 10;
                      if ( sceKernelIsKindOf(v11[0], TMAN_BSS_U32(0x544)) == 0 )
                      {
                        v9 = 11;
                        if ( sceKernelIsKindOf(v11[0], TMAN_BSS_U32(0x548)) == 0 )
                        {
                          v9 = 12;
                          if ( sceKernelIsKindOf(v11[0], TMAN_BSS_U32(0x518)) == 0 )
                          {
                            v9 = 13;
                            if ( sceKernelIsKindOf(v11[0], TMAN_BSS_U32(0x51C)) == 0 )
                            {
                              v9 = 14;
                              if ( sceKernelIsKindOf(v11[0], TMAN_BSS_U32(0x530)) == 0 )
                                v9 = -2147352366;
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    sceKernelCpuResumeIntr(v6);
    return v9;
  }
}


int sceKernelGetThreadmanIdOwner(int a1, int *a2)
{
  int v2;
  int v4;
  int v6;
  int v7;
  int v8;
  u32 *v10;
  int v11;
  u32 *i;
  u32 *v13;
  u32 *v14;
  int j;
  int k;

  v4 = (unsigned int)(a2 + 1) | (unsigned int)a2;
  if ( (v4 & (v2 << 11)) < 0 )
    return -2147352365;
  v6 = sceKernelCpuSuspendIntr(a1, v4);
  v7 = tman_mem_fn_9(a1);
  if ( v7 > 0 )
  {
    v8 = 5;
    goto LABEL_4;
  }
  v7 = tman_mem_fn_18(a1);
  if ( v7 > 0 )
  {
    v8 = 6;
    goto LABEL_4;
  }
  v7 = tman_mem_fn_3(a1);
  if ( v7 > 0 )
  {
    v8 = 7;
    goto LABEL_4;
  }
  v7 = tman_mem_fn_24(a1);
  if ( v7 > 0 )
  {
    v8 = 14;
LABEL_4:
    if ( a2 != NULL )
      *a2 = v8;
LABEL_6:
    sceKernelCpuResumeIntr(v6);
    return v7;
  }
  v10 = *(u32 **)TMAN_BSS_U32(0x50C);
  v7 = 0;
  if ( *(u32 *)TMAN_BSS_U32(0x50C) != TMAN_BSS_U32(0x50C) )
  {
    v11 = *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
    for ( i = &v10[v11]; ; i = &v10[v11] )
    {
      if ( i[76] == a1 )
      {
        v7 = i[2];
      }
      else if ( i[77] == a1 )
      {
        v7 = i[2];
      }
      v13 = (u32 *)i[52];
      if ( v13 != NULL )
      {
        for ( j = v13[2]; j != a1; j = v13[2] )
        {
          v13 = (u32 *)*v13;
          if ( v13 == NULL )
            goto LABEL_26;
        }
        v7 = i[2];
LABEL_26:
        v14 = (u32 *)i[53];
      }
      else
      {
        v14 = (u32 *)i[53];
      }
      if ( v14 != NULL )
      {
        for ( k = v14[2]; k != a1; k = v14[2] )
        {
          v14 = (u32 *)*v14;
          if ( v14 == NULL )
            goto LABEL_33;
        }
        v7 = i[2];
      }
LABEL_33:
      if ( v7 > 0 )
        break;
      v10 = (u32 *)*v10;
      if ( v10 == (u32 *)TMAN_BSS_U32(0x50C) )
        goto LABEL_36;
    }
    if ( a2 != NULL )
      *a2 = 1;
    goto LABEL_6;
  }
LABEL_36:
  sceKernelCpuResumeIntr(v6);
  return -2147352373;
}


// write access to const memory has been detected, the output may be wrong!
int ThreadManForKernel_D366D35A(int a1)
{
  if ( a1 != 0 )
  {
    *(u32 *)tman_thread_fn_23 = 65011720;
    *(volatile u32 *)&TMAN_BSS_U32(0x0) = 0;
  }
  return 0;
}


int tman_thread_fn_14()
{
  sceKernelCreateUIDtypeInherit("WaitQ", "Thread", 320, g_uid_thread_funcs, 0, &TMAN_BSS_U32(0x50C));
  return 0;
}


int module_reboot_before(int a1, int a2)
{
  int v2;

  v2 = sceKernelCpuSuspendIntr(a1, a2);
  sceKernelReleaseExceptionHandler(11, (void *)tman_vfpu_exc_handler);
  sceKernelReleaseIntrHandler(TMAN_BSS_U32(0x80C));
  sceKernelReleaseIntrHandler(65);
  sceKernelCpuResumeIntr(v2);
  return 0;
}


int sceKernelSetThreadmanLogHandler(int (*a1)(u32, u32, u32, u32), u32 *a2)
{
  TMAN_BSS_U32(0x48) = a1;
  if ( a2 != NULL )
    *a2 = TMAN_BSS_U32(0x40);
  return 0;
}


void tman_thread_fn_15(u32 *a1, unsigned int a2)
{
  u32 *v2;
  int v3;
  int v4;

  v2 = (u32 *)*a1;
  v3 = a1[1];
  if ( *a1 == v3 )
  {
    v4 = ~__builtin_rotateright32(0x80000000, a2);
    if ( v2 != a1 )
      (&TMAN_BSS_U32(0x40))[(a2 >> 5) + 34] &= v4;
  }
  v2[1] = v3;
  *(u32 *)a1[1] = v2;
}


void tman_thread_fn_16(u32 *a1, int a2)
{
  int *v3;
  unsigned int v4;
  unsigned int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int **v10;

  v3 = (int *)(&TMAN_BSS_U32(0x40))[0];
  v4 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 16);
  if ( a1[4] >= (signed int)v4 || TMAN_BSS_U32(0x4C) != 0 )
  {
    a1[3] = 2;
    tman_thread_fn_5(a1);
  }
  else
  {
    v5 = v4 >> 5;
    v6 = __builtin_rotateright32(0x80000000, v4);
    if ( a2 != 0 )
    {
      v7 = (&TMAN_BSS_U32(0x40))[2 * v4 + 38];
      v8 = (&TMAN_BSS_U32(0x40))[v5 + 34];
      TMAN_BSS_U32(0x44) = (int)a1;
      v9 = *(u32 *)(v7 + 4);
      (&TMAN_BSS_U32(0x40))[v5 + 34] = v8 | v6;
      v3[1] = v9;
      *(u32 *)(v7 + 4) = v3;
      v10 = (int **)v3[1];
      v3[3] = 2;
      *v3 = v7;
      a1[3] = 1;
      *v10 = v3;
    }
    else
    {
      a1[3] = 2;
      tman_thread_fn_5(a1);
      TMAN_BSS_U32(0x44) = 0;
    }
    tman_sched_fn_19(0, a2);
  }
}


u32 _sceKernelExitThread()
{
  u32 v0;

  v0 = sceKernelExitThread(*(u32 *)(*(u32 *)((&TMAN_BSS_U32(0x40))[0] + 36) + 24));
  tman_thread_fn_21((&TMAN_BSS_U32(0x40))[0] - 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21));
  return v0;
}


int sceKernelGetThreadId()
{
  int v0;
  int v1;
  int v2;
  int v3;

  v1 = 29;
  v2 = (&TMAN_BSS_U32(0x40))[0];
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(29, v0, 0, 0x20000u);
  v3 = -2147352476;
  tman_cfc0(13);
  if ( v1 == 0 )
    return *(u32 *)(v2 + 8);
  return v3;
}


int sceKernelGetThreadCurrentPriority()
{
  int v0;
  int v1;
  int v2;
  int v3;

  v1 = 30;
  v2 = (&TMAN_BSS_U32(0x40))[0];
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(30, v0, 1, *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 16));
  tman_cfc0(13);
  v3 = -2147352476;
  if ( v1 == 0 )
    return *(u32 *)(v2 + 16);
  return v3;
}


int sceKernelGetUserLevel()
{
  int v0;
  int v1;
  int v2;
  int v3;

  v1 = 37;
  v2 = (&TMAN_BSS_U32(0x40))[0];
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(37, v0, 1, (*(u32 *)((&TMAN_BSS_U32(0x40))[0] + 20) >> 28) ^ 8);
  tman_cfc0(13);
  v3 = -2147352476;
  if ( v1 == 0 )
    return (*(u32 *)(v2 + 20) >> 28) ^ 8;
  return v3;
}


int sceKernelIsUserModeThread()
{
  int v0;
  int v1;
  int v2;
  int v3;

  v1 = 38;
  v2 = (&TMAN_BSS_U32(0x40))[0];
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(38, v0, 1, *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 20) >> 31);
  tman_cfc0(13);
  v3 = -2147352476;
  if ( v1 == 0 )
    return *(u32 *)(v2 + 20) >> 31;
  return v3;
}


int tman_thread_fn_17(int a1, int a2)
{
  int v2;
  char *v3;
  int v4;
  int v5;
  u32 v7[4];

  v2 = (&TMAN_BSS_U32(0x40))[0];
  v3 = *(char **)((&TMAN_BSS_U32(0x40))[0] + 20);
  v4 = 0;
  if ( (int)v3 < 0 )
  {
    a2 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 48);
    v3 = (char *)v7 - a2;
    v7[0] = (char *)v7 - a2;
  }
  else
  {
    v5 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 44);
    if ( v5 == 0 )
      return v4;
    v7[0] = (char *)v7 - v5;
  }
  v4 = v7[0];
  if ( v7[0] < 64 )
  {
    sceKernelCpuSuspendIntr(v3, a2);
    SaveThreadContext(*(u32 *)(v2 + 28));
    tman_thread_fn_22(v2, *(u32 *)(v2 + 20) >> 31, *(u32 *)(*(u32 *)(v2 + 28) + 180));
    tman_sched_fn_19(1, 1);
    return v7[0];
  }
  return v4;
}


int sceKernelCheckThreadKernelStack(int a1, char *a2, int a3, int a4)
{
  int v4;
  int v5;
  int result;
  int v7;
  int v8;
  int v9;
  u32 v10[4];

  tman_cfc0(13);
  if ( v4 != 0 )
    return 0;
  if ( TMAN_BSS_U32(0x48) != NULL )
    ((int (*)())TMAN_BSS_U32(0x48))(43, v5, 0, a4);
  v7 = (&TMAN_BSS_U32(0x40))[0];
  v8 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 20);
  if ( v8 < 0 )
  {
    a2 = (char *)v10 - *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 48);
    v10[0] = a2;
  }
  else
  {
    v9 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 44);
    result = 0;
    if ( v9 == 0 )
      return result;
    v10[0] = (char *)v10 - v9;
  }
  if ( v10[0] < 64 )
  {
    sceKernelCpuSuspendIntr(v8, a2);
    SaveThreadContext(*(u32 *)(v7 + 28));
    tman_thread_fn_22(v7, *(u32 *)(v7 + 20) >> 31, *(u32 *)(*(u32 *)(v7 + 28) + 180));
    tman_sched_fn_19(1, 1);
  }
  return v10[0];
}


int sceKernelGetSystemStatusFlag()
{
  return TMAN_BSS_U32(0x810);
}


int sceKernelRegisterDeadThreadHandler(int a1)
{
  TMAN_BSS_U32(0x838) = a1;
  return 0;
}


int sceKernelReferSystemStatus(int *a1)
{
  int v1;
  int v3;
  int v4;
  int v5;
  int *v6;
  int v7;
  int v8;

  v3 = ((unsigned int)(a1 + 7) | (unsigned int)a1) & (v1 << 11);
  v4 = -2147352365;
  if ( v3 >= 0 )
  {
    v5 = sceKernelCpuSuspendIntr(v3, -2147352576);
    v6 = a1;
    v7 = 1;
    if ( TMAN_BSS_U32(0x48) != NULL )
      ((int (*)())TMAN_BSS_U32(0x48))(164, a1, 1, a1);
    tman_thread_fn_28(&TMAN_BSS_U32(0x714), v5, v7, v6);
    v8 = *a1;
    if ( TMAN_BSS_U32(0x714) < (unsigned int)*a1 )
      v8 = TMAN_BSS_U32(0x714);
    memcpy(a1, &TMAN_BSS_U32(0x714), v8);
    sceKernelCpuResumeIntr(v5);
    return 0;
  }
  return v4;
}


int sceKernelReferThreadProfiler(int a1, int a2, int a3)
{
  int v3;

  v3 = 0;
  tman_cfc0(13);
  if ( a3 == 0 && TMAN_BSS_U32(0x824) == 1 )
  {
    v3 = 1547698176;
    if ( *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 60) == 0 )
      return 0;
  }
  return v3;
}


int sceKernelReferGlobalProfiler()
{
  int v0;

  v0 = 1547698176;
  if ( TMAN_BSS_U32(0x824) != 2 )
    return 0;
  return v0;
}


int tman_thread_fn_18()
{
  sceKernelExitDeleteThread(0);
  return 0;
}


u32 tman_thread_fn_19()
{
  int v0;

  return sceKernelExitThread(v0);
}


u32 **tman_thread_fn_20()
{
  int *v0;
  u32 **result;
  int v2;
  unsigned int v3;
  int v4;
  int *v5;
  int v6;
  int **v7;
  u32 v8;
  u32 v9;
  u32 v10;
  int v11;
  int v12;
  u32 *v13;
  u32 *v14;
  int v15;
  u32 *v16;
  int v17;
  u32 *v18;
  u32 *v19;
  u32 *v20;
  int *v21;
  int v22;
  int v23;
  int v24;
  int **v25;

  v0 = (int *)(&TMAN_BSS_U32(0x40))[0];
  TMAN_BSS_U32(0x44) = (&TMAN_BSS_U32(0x40))[0];
  if ( TMAN_BSS_U32(0x74) != 0 )
  {
    result = (u32 **)TMAN_BSS_U32(0x550);
    TMAN_BSS_U32(0x74) = 0;
    *(u32 *)(TMAN_BSS_U32(0x550) + 12) = 1;
    v2 = v0[3];
    TMAN_BSS_U32(0x44) = (int)result;
    if ( v2 == 1 )
    {
      v3 = v0[4];
      v4 = (&TMAN_BSS_U32(0x40))[2 * v3 + 38];
      v5 = &(&TMAN_BSS_U32(0x40))[v3 >> 5];
      v0[1] = *(u32 *)(v4 + 4);
      *(u32 *)(v4 + 4) = v0;
      v6 = v5[34];
      v7 = (int **)v0[1];
      *v0 = v4;
      v0[3] = 2;
      v5[34] = v6 | __builtin_rotateright32(0x80000000, v3);
      *v7 = v0;
    }
  }
  else
  {
    v8 = _clz(TMAN_BSS_U32(0xD4));
    v9 = _clz(TMAN_BSS_U32(0xD0));
    if ( TMAN_BSS_U32(0xD0) == 0 )
      v9 = v8 + 32;
    v10 = _clz(TMAN_BSS_U32(0xCC));
    if ( TMAN_BSS_U32(0xCC) == 0 )
      v10 = v9 + 32;
    v11 = _clz(TMAN_BSS_U32(0xC8));
    if ( TMAN_BSS_U32(0xC8) == 0 )
      v11 = v10 + 32;
    if ( v11 >= 128 )
    {
      return (u32 **)Kprintf("Panic: not found executable Thread\n");
    }
    else
    {
      result = *(u32 ***)((&TMAN_BSS_U32(0x40))[0] + 12);
      v12 = 2 * v11;
      if ( result == (u32 **)((char *)tman_idle_loop + 1) )
      {
        v17 = *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 16);
        result = (u32 **)((char *)&TMAN_BSS_U8(0xD8) + v12 * 4);
        if ( v11 < v17 )
        {
          v18 = *result;
          v19 = (u32 *)**result;
          v20 = (u32 *)(*result)[1];
          if ( v19 == v20 && v19 != v18 )
            (&TMAN_BSS_U32(0x40))[((unsigned int)v11 >> 5) + 34] &= ~__builtin_rotateright32(0x80000000, v11);
          v19[1] = v20;
          *(u32 *)v18[1] = v19;
          v21 = &(&TMAN_BSS_U32(0x40))[(unsigned int)v17 >> 5];
          v22 = (&TMAN_BSS_U32(0x40))[2 * v17 + 38];
          v23 = v21[34];
          v0[1] = *(u32 *)(v22 + 4);
          v24 = v23 | __builtin_rotateright32(0x80000000, v17);
          *(u32 *)(v22 + 4) = v0;
          v25 = (int **)v0[1];
          v18[3] = 1;
          *v0 = v22;
          v21[34] = v24;
          TMAN_BSS_U32(0x44) = (int)v18;
          v0[3] = 2;
          *v25 = v0;
        }
      }
      else
      {
        v13 = (u32 *)(&TMAN_BSS_U32(0x40))[v12 + 38];
        v14 = (u32 *)*v13;
        v15 = v13[1];
        if ( *v13 == v15 && v14 != v13 )
          (&TMAN_BSS_U32(0x40))[((unsigned int)v11 >> 5) + 34] &= ~__builtin_rotateright32(0x80000000, v11);
        v14[1] = v15;
        v16 = (u32 *)v13[1];
        TMAN_BSS_U32(0x44) = (int)v13;
        *v16 = v14;
        v13[3] = 1;
      }
    }
  }
  return result;
}


int tman_thread_fn_21(int a1)
{
  u32 *v2;
  u32 *v3;
  int v4;
  int v5;
  int v6;
  int v7;
  int v8;
  const char *v9;
  u32 *v10;
  int v11;
  int (*v12)(u32, u32);
  u32 *v13;

  v2 = (u32 *)(a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21));
  if ( TMAN_BSS_U32(0x4C) != 0 )
  {
    TMAN_BSS_U32(0x4C) = 0;
    Kprintf("thread_is_dead: Thread dispatch is disabled!\n");
  }
  v3 = (u32 *)*v2;
  v4 = v2[1];
  if ( *v2 == v4 && v3 != v2 )
    (&TMAN_BSS_U32(0x40))[(v2[4] >> 5) + 34] &= ~__builtin_rotateright32(0x80000000, v2[4]);
  v3[1] = v4;
  *(u32 *)v2[1] = v3;
  tman_sched_fn_9(a1, -2147352148);
  tman_sched_fn_22((int)v2);
  tman_sched_fn_25((int)v2);
  tman_thread_fn_1((int)v2, v5, v6, v7);
  tman_sync_fn_6((int)v2);
  tman_sync_fn_12((int)v2);
  tman_sched_fn_24(v2);
  tman_mem_fn_22((int)v2, v8);
  v9 = *(const char **)(a1 + 16);
  if ( v9 == NULL )
    v9 = (const char *)"";
  Kprintf("Thread \"%s\" (thid=0x%08x) is DEAD\n", v9, v2[2]);
  v10 = (u32 *)v2[7];
  if ( v10[9] == 3 )
  {
    Kprintf(" Saved registers:\n");
    Kprintf(
      " at=%08x  v0-1=%08x,%08x  a0-3=%08x,%08x,%08x,%08x\n",
      v10[17],
      v10[18],
      v10[19],
      v10[20],
      v10[21],
      v10[22],
      v10[23]);
    Kprintf(
      " t0-7=%08x,%08x,%08x,%08x, %08x,%08x,%08x,%08x\n",
      v10[24],
      v10[25],
      v10[26],
      v10[27],
      v10[28],
      v10[29],
      v10[30],
      v10[31]);
    Kprintf(
      " s0-7=%08x,%08x,%08x,%08x, %08x,%08x,%08x,%08x\n",
      v10[32],
      v10[33],
      v10[34],
      v10[35],
      v10[36],
      v10[37],
      v10[38],
      v10[39]);
    Kprintf(
      " t8=%08x t9=%08x\tk0=%08x k1=%08x\t  gp=%08x sp=%08x\n",
      v10[40],
      v10[41],
      v10[42],
      v10[43],
      v10[44],
      v10[45]);
    Kprintf(" fp=%08x ra=%08x\tlo=%08x hi=%08x\t  PC=%08x\n", v10[46], v10[47], v10[12], v10[11], v10[14]);
  }
  v11 = v2[9];
  if ( v11 != 0 )
    Kprintf(" Syscall return address = 0x%08x, sp = 0x%08x\n", *(u32 *)(v11 + 12), *(u32 *)(v11 + 8));
  Kprintf("\n");
  v12 = (int (*)(u32, u32))TMAN_BSS_U32(0x838);
  v2[1] = TMAN_BSS_U32(0x4FC);
  TMAN_BSS_U32(0x4FC) = (int)v2;
  v13 = (u32 *)v2[1];
  *v2 = &TMAN_BSS_U32(0x4F8);
  v2[21] = -2147352158;
  v2[3] = 32;
  *v13 = v2;
  TMAN_BSS_U32(0x44) = 0;
  if ( v12 == NULL )
    _break(0, 0);
  return v12(v2[2], v2[7]);
}


int tman_thread_fn_22(u32 *a1, int a2, int a3)
{
  int result;
  const char *v6;
  int v7;
  int v8;

  result = a1[3];
  if ( result != 32 )
  {
    v6 = "kernel ";
    if ( a2 != 0 )
    {
      v7 = a1[12];
      v8 = a1[51];
    }
    else
    {
      v7 = a1[11];
      v8 = a1[49];
      v6 = (const char *)"";
    }
    Kprintf(
      "\nThread (thid=0x%08x) %sstack overflow\n Stack = 0x%08x, Stack size = 0x%08x, SP = 0x%08x\n",
      a1[2],
      v6,
      v7,
      v8,
      a3);
    return tman_thread_fn_21((int)&a1[-*(u8 *)(TMAN_BSS_U32(0x50C) + 21)]);
  }
  return result;
}


int tman_thread_fn_23(int a1)
{
  int result;

  if ( sceKernelGetCompiledSdkVersion(a1) <= 100663295 )
    return ThreadManForKernel_D366D35A(1);
  result = *(u32 *)(a1 + 12);
  if ( result != 32 )
  {
    Kprintf("\nThread (thid=0x%08x) k0 reg corrupted\n", *(u32 *)(a1 + 8));
    return tman_thread_fn_21(a1 - 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21));
  }
  return result;
}


void tman_thread_fn_24(int a1, int a2, int a3)
{
  int v5;
  u32 *v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  u32 *v13;

  v5 = a1;
  sceKernelCpuSuspendIntr(a1, a2);
  v6 = (u32 *)(&TMAN_BSS_U32(0x40))[0];
  v7 = (&TMAN_BSS_U32(0x40))[0] - 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
  if ( TMAN_BSS_U32(0x4C) != 0 )
  {
    TMAN_BSS_U32(0x4C) = 0;
    Kprintf("doExitDeleteThread: Thread dispatch is disabled!\n");
  }
  v8 = a2;
  if ( v5 != 0 )
  {
    v6[34] |= 8u;
    v8 = a2;
  }
  tman_sched_fn_20(4, (int)v6, v8);
  tman_thread_fn_1((int)v6, v9, v10, v11);
  tman_sched_fn_9(v7, a3);
  tman_sync_fn_6((int)v6);
  tman_sync_fn_12((int)v6);
  tman_sched_fn_24(v6);
  tman_mem_fn_22((int)v6, v12);
  if ( (v6[34] & 8) != 0 )
    v5 = 1;
  if ( v5 != 0 )
  {
    tman_sched_fn_20(8, (int)v6, a2);
    v6[3] = 0;
    tman_thread_fn_4(v6[2]);
  }
  else
  {
    v6[1] = TMAN_BSS_U32(0x4EC);
    TMAN_BSS_U32(0x4EC) = (int)v6;
    v13 = (u32 *)v6[1];
    *v6 = &TMAN_BSS_U32(0x4E8);
    v6[3] = 16;
    *v13 = v6;
  }
  TMAN_BSS_U32(0x44) = 0;
  tman_sched_fn_19(1, 1);
  Kprintf("panic ! Thread DORMANT !\n");
  _break(0, 1u);
}


int tman_thread_fn_25(int a1, int a2, int a3)
{
  int v3;
  int v7;
  int v8;
  int v9;
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
  u32 *v22;
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
  int *v36;
  int v37;
  s64 v38;
  int v39;
  int v40;
  int v41;
  int v42;
  int v43;
  int v44;

  tman_cfc0(13);
  v7 = v3 != 0;
  v8 = -2147352476;
  v44 = 0;
  v9 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
  if ( a2 == 0 || !v7 )
  {
    v10 = v9 ^ (&TMAN_BSS_U32(0x40))[0];
    if ( v3 == 0 )
    {
      v8 = -2147352476;
      if ( *(u32 *)((&TMAN_BSS_U32(0x40))[0] + 136) != 0 )
        return v8;
      v10 = v9 ^ (&TMAN_BSS_U32(0x40))[0];
    }
    v11 = v10 == 0;
    v8 = -2147352169;
    if ( v3 != 0 || !v11 )
    {
      v12 = *(u32 *)(v9 + 12);
      v8 = -2147352168;
      if ( v12 != 0 )
      {
        if ( v12 == 0x10 || v12 == 0x20 )
        {
          v8 = -2147352158;
          if ( a2 != 0 )
          {
            tman_sched_fn_20(8, v9, a3);
            v13 = *(u32 *)v9;
            v14 = *(u32 *)(v9 + 4);
            *(u32 *)(v9 + 12) = 0;
            *(u32 *)(v13 + 4) = v14;
            **(u32 **)(v9 + 4) = v13;
            tman_thread_fn_12((u32 *)v9, a3);
            sceKernelDeleteUID(*(u32 *)(v9 + 8));
            return 0;
          }
          return v8;
        }
        if ( v7 && v11 )
        {
          if ( TMAN_BSS_U32(0x4C) != 0 )
          {
            v8 = 0;
            TMAN_BSS_U32(0x4C) = 3;
            return v8;
          }
          v16 = *(u32 *)(v9 + 136);
        }
        else
        {
          v16 = *(u32 *)(v9 + 136);
        }
        if ( (v16 & 0xC) != 0 )
        {
          v8 = -2147352158;
          if ( a2 != 0 )
          {
            v8 = 0;
            *(u32 *)(v9 + 136) = v16 | 8;
          }
          return v8;
        }
        *(u32 *)(v9 + 84) = -2147352148;
        if ( TMAN_BSS_U32(0x4C) != 0 )
        {
          TMAN_BSS_U32(0x4C) = 0;
          Kprintf("doTerminateDeleteThread: Thread dispatch is disabled!\n");
        }
        tman_sched_fn_22(v9);
        tman_sched_fn_25(v9);
        if ( tman_thread_fn_1(v9, v17, v18, v19) != 0 )
        {
          v20 = *(u32 *)(v9 + 12);
        }
        else
        {
          if ( tman_sched_fn_23(4, v9) == 0 )
          {
            tman_sched_fn_9(a1, -2147352148);
            tman_sync_fn_6(v9);
            tman_sync_fn_12(v9);
            tman_sched_fn_24((u32 *)v9);
            tman_mem_fn_22(v9, v33);
            if ( v7 && v10 == 0 )
            {
              TMAN_BSS_U32(0x44) = 0;
            }
            else
            {
              v34 = *(u32 *)(v9 + 12);
              if ( v34 == 2 )
              {
                v42 = *(u32 *)v9;
                v43 = *(u32 *)(v9 + 4);
                if ( *(u32 *)v9 == v43 && v42 != v9 )
                  (&TMAN_BSS_U32(0x40))[(*(u32 *)(v9 + 16) >> 5) + 34] &= ~__builtin_rotateright32(
                                                                      0x80000000,
                                                                      *(u32 *)(v9 + 16));
                *(u32 *)(v42 + 4) = v43;
                **(u32 **)(v9 + 4) = v42;
              }
              else
              {
                v37 = *(u32 *)v9;
                *(u32 *)(v37 + 4) = *(u32 *)(v9 + 4);
                v38 = ((u64)(v38) & 0x00000000FFFFFFFFULL) | ((u64)(u32)(v34 == 4) << 32);
                v38 = ((u64)(v38) & 0xFFFFFFFF00000000ULL) | (u32)(v34 == 0xC);
                **(u32 **)(v9 + 4) = v37;
                if ( v38 != 0 )
                {
                  tman_sched_fn_10((u32 *)v9);
                  v41 = *(u32 *)(v9 + 92);
                  if ( v41 != 0 )
                    tman_sched_fn_11(v41, v39, v40);
                }
              }
            }
            v35 = TMAN_BSS_U32(0x4EC);
            *(u32 *)v9 = &TMAN_BSS_U32(0x4E8);
            *(u32 *)(v9 + 4) = v35;
            TMAN_BSS_U32(0x4EC) = v9;
            v36 = *(int **)(v9 + 4);
            *(u32 *)(v9 + 12) = 16;
            *v36 = v9;
            goto LABEL_32;
          }
          v20 = *(u32 *)(v9 + 12);
        }
        if ( (unsigned int)(v20 - 1) >= 2 )
        {
          v21 = *(u32 *)v9;
          *(u32 *)(v21 + 4) = *(u32 *)(v9 + 4);
          **(u32 **)(v9 + 4) = v21;
          if ( v20 == 4 || v20 == 0xC )
          {
            tman_sched_fn_10((u32 *)v9);
            v32 = *(u32 *)(v9 + 92);
            if ( v32 != 0 )
              tman_sched_fn_11(v32, v30, v31);
          }
        }
        tman_thread_fn_8(*(u32 **)(v9 + 28));
        v22 = *(u32 **)(v9 + 28);
        v23 = *(u32 *)(v9 + 220);
        v22[9] = 3;
        v22[21] = 1;
        v22[22] = -2147352148;
        v22[42] = v23;
        v22[20] = 0;
        if ( *(int *)(v9 + 20) >= 0 )
          v24 = *(u32 *)(v9 + 200);
        else
          v24 = *(u32 *)(v9 + 40);
        v22[45] = v24;
        v25 = v22[45];
        v22[47] = tman_thread_fn_19;
        v22[46] = v25;
        v26 = *(u32 *)(v9 + 216);
        v22[14] = tman_thread_fn_24;
        v22[44] = v26;
        v22[13] = 3;
        if ( *(u32 *)(v9 + 12) != 2 )
        {
          *(u32 *)(v9 + 12) = 2;
          tman_thread_fn_5((u32 *)v9);
          TMAN_BSS_U32(0x44) = 0;
        }
        if ( v3 == 0 )
        {
          v29 = tman_sched_fn_5(a1, 9, 0, 0, 0);
          v44 = v29;
          if ( v29 == -2147352148 )
            v44 = 0;
        }
LABEL_32:
        if ( a2 != 0 )
        {
          tman_sched_fn_20(8, v9, a3);
          v27 = *(u32 *)v9;
          v28 = *(u32 *)(v9 + 4);
          *(u32 *)(v9 + 12) = 0;
          *(u32 *)(v27 + 4) = v28;
          **(u32 **)(v9 + 4) = v27;
          tman_thread_fn_12((u32 *)v9, a3);
          sceKernelDeleteUID(*(u32 *)(v9 + 8));
        }
        return v44;
      }
    }
  }
  return v8;
}


int tman_thread_fn_26(int a1, u32 *a2, int a3, int a4, unsigned int a5)
{
  u32 *v9;
  int v10;
  u32 *v11;
  u32 *v12;
  int v13;
  int v14;
  u32 *v15;
  int v16;
  int *v17;
  int v18;
  int v19;
  int v20;
  int v21;

  tman_cfc0(13);
  v9 = (u32 *)(&TMAN_BSS_U32(0x40))[0];
  v10 = -2147352476;
  if ( a1 == 0 )
  {
    v10 = -2147352365;
    if ( a4 >= 0 )
    {
      v11 = (u32 *)sceKernelAllocHeapMemory(TMAN_BSS_U32(0x820), 28);
      v10 = -2147352176;
      if ( v11 != NULL )
      {
        v12 = a2;
        v13 = -1;
        if ( (v9[5] & 0x100000) == 0 )
          sceKernelMemset32(a2, -1, a3);
        *a2 = v9[2];
        v14 = sceKernelCpuSuspendIntr(v12, v13);
        v15 = (u32 *)v9[9];
        v11[2] = v9[76];
        v16 = ((unsigned int)a4 >> 2) & 0x3FFFFFF;
        v11[3] = v9[49];
        v11[4] = v15[1];
        v17 = (u32 *)((char *)a2 + a3 - 64);
        v18 = v16 | 0xC000000;
        v11[5] = v15[3];
        v19 = v9[53];
        v11[6] = v15[2];
        v9[76] = 0;
        v9[49] = a3;
        *v17 = HIWORD(a5) | 0x3C040000;
        v17[2] = (u16)a5 | 0x34840000;
        v17[3] = -1883307976;
        v17[1] = v18;
        v17[4] = -1883439044;
        v20 = TMAN_BSS_U32(0x818);
        v21 = v9[11];
        *v11 = v19;
        v11[1] = v21;
        v17[5] = (v20 << 6) & 0x3FFFFC0 | 0xC;
        v17[6] = 65011720;
        v17[7] = 0;
        v17[8] = 0;
        v17[9] = 0;
        v17[10] = 0;
        v17[11] = 0;
        v17[12] = 0;
        v9[53] = v11;
        v9[11] = a2;
        v17[13] = 0;
        v17[14] = v11[5];
        v17[15] = v11[6];
        v15[2] = v17;
        v15[3] = 0;
        v15[1] = v17;
        _cache(8u, v17);
        _cache(0x1Bu, v17);
        sceKernelCpuResumeIntrWithSync(v14);
        return 0;
      }
    }
  }
  return v10;
}


int tman_thread_fn_27(
        int a1,
        int a2,
        float a3,
        float a4,
        float a5,
        float a6,
        float a7,
        float a8,
        float a9,
        float a10)
{
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
  int v24;
  s64 v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  int v32;
  int v33;
  int v34;
  u32 *v35;
  int v36;
  int v37;
  int v39;
  s64 v40;

  v11 = a2 - 244;
  v12 = a1 + 4 * *(u8 *)(TMAN_BSS_U32(0x50C) + 21);
  do
  {
    v11 += 4;
    *(u32 *)(v11 + 240) = 0;
  }
  while ( v11 != a2 );
  v13 = *(u32 *)(a1 + 16);
  *(u32 *)a2 = 244;
  if ( v13 != 0 )
    strncpy(a2 + 4, v13, 31);
  v14 = *(u32 *)(v12 + 192);
  v15 = *(s16 *)(v12 + 184);
  *(u32 *)(a2 + 36) = *(u32 *)(v12 + 20);
  v16 = *(u32 *)(v12 + 12);
  *(u32 *)(a2 + 44) = v14;
  *(u32 *)(a2 + 40) = v16;
  v17 = *(u32 *)(v12 + 44);
  *(u32 *)(a2 + 52) = *(u32 *)(v12 + 196);
  v18 = *(u32 *)(v12 + 204);
  *(u32 *)(a2 + 48) = v17;
  *(u32 *)(a2 + 60) = v18;
  v19 = *(u32 *)(v12 + 216);
  v20 = *(u32 *)(v12 + 292);
  *(u32 *)(a2 + 56) = *(u32 *)(v12 + 48);
  *(u32 *)(a2 + 64) = v19;
  *(u32 *)(a2 + 68) = v20;
  *(u32 *)(a2 + 76) = v15;
  v21 = *(u32 *)(v12 + 16);
  *(u32 *)(a2 + 72) = *(u32 *)(v12 + 296);
  *(u32 *)(a2 + 80) = v21;
  if ( v16 == 4 || v16 == 0xC )
  {
    if ( *(u32 *)(v12 + 152) != 0 )
      v22 = *(u32 *)(v12 + 88) + 256;
    else
      v22 = *(u32 *)(v12 + 88);
    *(u32 *)(a2 + 84) = v22;
    v23 = *(u32 *)(v12 + 92);
    if ( v23 != 0 )
      *(u32 *)(a2 + 88) = *(u32 *)(v23 + 12) ^ TMAN_BSS_U32(0x844);
    else
      *(u32 *)(a2 + 88) = 0;
  }
  v24 = (&TMAN_BSS_U32(0x40))[0];
  *(u32 *)(a2 + 92) = *(u32 *)(v12 + 80);
  v25 = *(u64 *)(v12 + 52);
  if ( v24 == v12 )
  {
    v40 = ((u64)(v40) & 0xFFFFFFFF00000000ULL) | (u32)(tman_mem_fn_32());
    v25 += v40 - TMAN_BSS_U64(0xB0);
  }
  *(u64 *)(a2 + 96) = v25;
  v26 = *(u32 *)(v12 + 28);
  *(u32 *)(a2 + 104) = *(u32 *)(v12 + 68);
  v27 = *(u32 *)(v12 + 72);
  *(u32 *)(a2 + 120) = v26;
  *(u32 *)(a2 + 108) = v27;
  *(u32 *)(a2 + 112) = *(u32 *)(v12 + 76);
  v28 = *(u32 *)(v12 + 156);
  *(u32 *)(a2 + 116) = v28;
  tman_cfc0(8);
  if ( v28 != 0 )
  {
    tman_sched_fn_1(v28, a3, a4, a5, a6, a7, a8, a9, a10);
    tman_ctc0(8, 0);
  }
  v29 = *(u32 *)(a2 + 120);
  v30 = *(u16 *)(v29 + 36);
  *(u32 *)(v29 + 36) = v30;
  if ( *(u32 *)(v12 + 24) != 0 || (*(u32 *)(v29 + 52) & 0x20000000) != 0 )
  {
    *(u32 *)(v29 + 36) = v30 | 0x1000000;
    v31 = *(u32 *)(v12 + 32);
  }
  else
  {
    v31 = *(u32 *)(v12 + 32);
  }
  *(u32 *)(a2 + 124) = v31;
  v32 = *(u32 *)(v12 + 32);
  if ( v32 != 0 )
  {
    if ( TMAN_BSS_U32(0x5C) == v32 )
      *(u32 *)(a2 + 124) = 0;
    v33 = *(u32 *)(v12 + 36);
  }
  else
  {
    v33 = *(u32 *)(v12 + 36);
  }
  if ( v33 != 0 )
    *(u32 *)(a2 + 128) = *(u32 *)(v33 + 12);
  else
    *(u32 *)(a2 + 128) = 0;
  v34 = *(u32 *)(v12 + 120);
  *(u32 *)(a2 + 136) = v33;
  if ( v34 > 0 )
  {
    v35 = *(u32 **)(v12 + 124);
    if ( v35 == NULL )
    {
      v36 = *(u8 *)(v12 + 88);
      goto LABEL_34;
    }
    *(u32 *)(a2 + 140) = *v35;
  }
  v36 = *(u8 *)(v12 + 88);
LABEL_34:
  switch ( v36 )
  {
    case 0:
    case 1:
    case 5:
    case 7:
    case 9:
    case 10:
    case 11:
    case 14:
      goto LABEL_36;
    case 2:
    case 4:
      *(u32 *)(a2 + 144) = *(u32 *)(v12 + 96);
      *(u32 *)(a2 + 148) = *(u32 *)(v12 + 100);
      goto LABEL_36;
    case 3:
    case 12:
    case 13:
      v39 = *(u32 *)(v12 + 96);
      goto LABEL_42;
    case 6:
      v39 = *(u32 *)(v12 + 100);
      goto LABEL_42;
    case 8:
      *(u32 *)(a2 + 148) = *(u32 *)(v12 + 96);
      *(u32 *)(a2 + 152) = *(u32 *)(v12 + 104);
      *(u32 *)(a2 + 156) = *(u32 *)(v12 + 112);
      *(u32 *)(a2 + 160) = *(u32 *)(v12 + 100);
      v39 = *(u32 *)(v12 + 116);
LABEL_42:
      *(u32 *)(a2 + 144) = v39;
LABEL_36:
      v37 = *(u32 *)(v12 + 60);
      break;
    default:
      v37 = *(u32 *)(v12 + 60);
      break;
  }
  if ( v37 != 0 )
    *(u32 *)(a2 + 132) = v37 + 16;
  else
    *(u32 *)(a2 + 132) = 0;
  *(u32 *)(a2 + 176) = *(u32 *)(v12 + 220);
  return memcpy(a2 + 180, v12 + 224, 64);
}


int tman_thread_fn_28(u32 *a1, int a2)
{
  u32 *v2;
  int v3;
  int v4;
  int result;

  v2 = a1 - 7;
  do
  {
    ++v2;
    v2[6] = 0;
  }
  while ( v2 != a1 );
  *a1 = 28;
  tman_cfc0(13);
  if ( v2 != NULL )
  {
    v3 = 4;
  }
  else
  {
    v3 = 3;
    if ( a2 != 0 )
    {
      if ( TMAN_BSS_U32(0x4C) == 0 )
      {
        a1[1] = 0;
        goto LABEL_6;
      }
      v3 = 1;
    }
  }
  a1[1] = v3;
LABEL_6:
  v4 = TMAN_BSS_U32(0x54C);
  a1[2] = *(u32 *)(TMAN_BSS_U32(0x54C) + 52);
  a1[3] = *(u32 *)(v4 + 56);
  a1[5] = TMAN_BSS_U32(0x50);
  result = TMAN_BSS_U32(0x58);
  a1[6] = TMAN_BSS_U32(0x58);
  a1[4] = *(u32 *)(v4 + 68);
  return result;
}


int tman_thread_fn_29(int a1, int a2, int a3, int a4)
{
  u32 *v6;
  int v7;
  int v8;
  int v9;

  sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
  v6 = (u32 *)(a1 + 4 * *(u8 *)(a2 + 21));
  sceKernelMemset32(v6, 0, 320);
  v7 = TMAN_BSS_U32(0x844);
  v8 = *(u32 *)(a1 + 12);
  v6[3] = 0;
  v6[2] = v8 ^ v7;
  v9 = *(u32 *)(a1 + 12);
  v6[1] = v6;
  *v6 = v6;
  v6[42] = v6 + 42;
  v6[44] = v6 + 44;
  v6[36] = v6 + 36;
  v6[43] = v6 + 42;
  v6[45] = v6 + 44;
  v6[37] = v6 + 36;
  return v9 ^ v7;
}


int tman_thread_fn_30(int a1, int a2, int a3, int a4)
{
  int v8;
  int *v9;
  int v10;
  int *v11;
  int v12;
  int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v19;

  v10 = sceKernelCpuSuspendIntr(a1, a2);
  v11 = (int *)(a1 + 4 * *(u8 *)(a2 + 21));
  if ( v11[3] != 0 && (v12 = tman_thread_fn_25(a1, 1, v10)) != 0 )
  {
    sceKernelCpuResumeIntr(v10);
    return v12;
  }
  else
  {
    tman_thread_fn_13(v11, v8, v9);
    if ( v11[11] != 0 )
      sceKernelFreePartitionMemory(v11[76]);
    v14 = v11[5];
    if ( v14 < 0 )
    {
      if ( v11[12] != 0 )
        sceKernelFreePartitionMemory(v11[77]);
      v19 = v11[15];
      if ( v19 != 0 )
        sceKernelFreeHeapMemory(TMAN_BSS_U32(0x820), v19, v14);
    }
    tman_cfc0(8);
    v15 = v11[7];
    if ( v15 == v13 )
      tman_ctc0(8, 0);
    if ( v15 != 0 )
      sceKernelFreeHeapMemory(TMAN_BSS_U32(0x820), v15, v14);
    v16 = v11[8];
    if ( v16 != 0 )
    {
      if ( TMAN_BSS_U32(0x5C) == v16 )
        TMAN_BSS_U32(0x5C) = 0;
      tman_vfpu_flush();
      sceKernelFreeHeapMemory(TMAN_BSS_U32(0x820), v11[8], v17);
    }
    sceKernelCpuResumeIntr(v10);
    sceKernelCallUIDObjCommonFunction(a1, a2, a3, a4);
    return 0;
  }
}



/* Exported variable & leading-underscore / CB aliases */
u32 ThreadManForKernel_921B7ACD = 0;

int _sceKernelAllocateTlspl(int a1, int a2, int a3) { return ((int (*)())sceKernelAllocateTlspl)(a1, a2, a3); }
int _sceKernelCheckThreadStack(void) { return ((int (*)())sceKernelCheckThreadStack)(); }
int _sceKernelLockLwMutexCB(int a1, int a2, int a3) { return ((int (*)())sceKernelLockLwMutexCB)(a1, a2, a3); }
int _sceKernelReturnFromCallback(void) { return ((int (*)())sceKernelReturnFromCallback)(); }
int _sceKernelReturnFromChangeStack(void) { return ((int (*)())sceKernelReturnFromChangeStack)(); }
int _sceKernelReturnFromTimerHandler(void) { return ((int (*)())sceKernelReturnFromTimerHandler)(); }
int _sceKernelTryLockLwMutex(int a1, int a2) { return ((int (*)())sceKernelTryLockLwMutex)(a1, a2); }
int _sceKernelUnlockLwMutex(int a1, int a2) { return ((int (*)())sceKernelUnlockLwMutex)(a1, a2); }
int _sceKernelLockLwMutex(int a1, int a2, int a3) { return ((int (*)())sceKernelLockLwMutexCB)(a1, a2, a3); }

int sceKernelWaitThreadEndCB(int a1, int a2) { return ((int (*)())sceKernelWaitThreadEnd)(a1, a2, 1); }
int sceKernelWaitSemaCB(int a1, int a2, int a3) { return ((int (*)())sceKernelWaitSema)(a1, a2, a3, 1); }
int sceKernelReceiveMbxCB(int a1, int a2, int a3) { return ((int (*)())sceKernelReceiveMbx)(a1, a2, a3, 1); }
int sceKernelReceiveMsgPipeCB(int a1, int a2, int a3, int a4, int a5, int a6) { return ((int (*)())sceKernelReceiveMsgPipe)(a1, a2, a3, a4, a5, a6, 1); }
int sceKernelAllocateVplCB(int a1, int a2, int a3, int a4) { return ((int (*)())sceKernelAllocateVpl)(a1, a2, a3, a4, 1); }
int sceKernelAllocateFplCB(int a1, int a2, int a3) { return ((int (*)())sceKernelAllocateFpl)(a1, a2, a3, 1); }
int sceKernelReferThreadDebugStatus(int a1, int a2) { return ((int (*)())sceKernelReferThreadStatus)(a1, a2); }
