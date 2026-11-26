/* Copyright (C) 2011, 2012, 2013 The uOFW team    
   See the file COPYING for copying permission.
*/

#include "usersystemlib_int.h"

#include <interruptman.h>
#include <sysmem_kernel.h>
#include <threadman_user.h>

#define STACK_SIZE 		0x2000
#define CMD_LIST_SIZE 	0x500

SCE_MODULE_INFO(
	"sceKernelLibrary",
    SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD | SCE_MODULE_ATTR_EXCLUSIVE_START,
    1, 6
);
SCE_MODULE_START_THREAD_PARAMETER(3, SCE_KERNEL_MODULE_INIT_PRIORITY, 0x400, 0);
SCE_MODULE_BOOTSTART("_UsersystemLibInit");
SCE_SDK_VERSION(SDK_VERSION);

static s32 g_stackSize = STACK_SIZE;
SceGeLazy g_lazy = {
    .dlId = -1,
    .stall = NULL,
    .count = 0,
    .max = 100
};
static u8 g_stack[STACK_SIZE];
SceThread *g_thread;
static s32 g_cmdList[CMD_LIST_SIZE];

// module_start
s32 _UsersystemLibInit(SceSize argSize __attribute__((unused)), const void *argBlock __attribute__((unused)))
{
    // InterruptManager_EEE43F47
    sceKernelRegisterUserSpaceIntrStack(
        (s32)g_stack, // 0xBC0
        g_stackSize, // 0x2000
        (s32)&g_thread // 0x2BC0
    );

    // SysMemUserForUser_A6848DF8
    sceKernelSetUsersystemLibWork(
        g_cmdList, // 0x2C00
        sceGe_lazy_31129B95, // 0x140
        &g_lazy // 0xB80
    );

    return SCE_ERROR_OK;
}
