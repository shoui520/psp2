/* Copyright (C) 2011, 2012, 2013 The uOFW team
   See the file COPYING for copying permission.
*/

#include <common_imp.h>

#include <sysmem_kernel.h>
#include <sysmem_sysclib.h>
#include <sysmem_utils_kernel.h>

#include "gamePatching.h"
#include "libcUtils.h"

#define IS_JUMP(op) (((op) & 0xFC000000) == 0x0C000000)
#define JUMP(ref, addr) (((ref) & 0xF0000000) | (((addr) & 0x03FFFFFF) << 2))
#define MAKE_JAL(addr) (0x0C000000 | (((u32)(addr) >> 2) & 0x03FFFFFF))

#define MEDAL_OF_HONORS_HEROES_GAME_ID_US               "ULUS10141"
#define MEDAL_OF_HONORS_HEROES_GAME_ID_UK               "ULAS42082"
#define MEDAL_OF_HONORS_HEROES_GAME_ID_EU               "ULES00557"
#define MEDAL_OF_HONORS_HEROES_GAME_ID_FR               "ULES00558"
#define MEDAL_OF_HONORS_HEROES_GAME_ID_DE               "ULES00559"
#define MEDAL_OF_HONORS_HEROES_GAME_ID_EU_PLATIN        "ULES00560"
#define MEDAL_OF_HONORS_HEROES_GAME_ID_SP               "ULES00561"
#define MEDAL_OF_HONORS_HEROES_GAME_ID_NL               "ULES00562"
#define MEDAL_OF_HONORS_HEROES_GAME_ID_KOR              "ULKS46066"
#define MEDAL_OF_HONORS_HEROES_GAME_ID_JAP              "ULJM05213"

typedef struct {
    char *gameId;
    s32 **strcpy256;
    s32 numStrcpy256;
    s32 **wcscpy256;
    s32 numWcscpy256;
} SceJumpFixup;

s32 *g_mohhUsEuStrcpyFixups[] = { (s32 *)0x089C07AC };
s32 *g_mohhUsEuWcscpyFixups[] = { 
    (s32 *)0x0888EA48, (s32 *)0x0888EA84, (s32 *)0x0888EAB4, (s32 *)0x0888EAF0, (s32 *)0x0888EB20, (s32 *)0x0888EB88, 
    (s32 *)0x0888EBB8
};
s32 *g_mohhJpStrcpyFixups[] = { (s32 *)0x089C08F0 };
s32 *g_mohhJpWcscpyFixups[] = { 
    (s32 *)0x0888EA40, (s32 *)0x0888EA7C, (s32 *)0x0888EAAC, (s32 *)0x0888EAE8, (s32 *)0x0888EB18, (s32 *)0x0888EB80, 
    (s32*)0x0888EBB0
};
s32 *g_mohhUkKrStrcpyFixups[] = { (s32 *)0x089C0928 };
s32 *g_mohhUkKrWcscpyFixups[] = { 
    (s32 *)0x0888EA48, (s32 *)0x0888EA84, (s32 *)0x0888EAB4, (s32 *)0x0888EAF0, (s32 *)0x0888EB20, (s32 *)0x0888EB88, 
    (s32 *)0x0888EBB8
};

#define INIT_PATCH_GAMES_NUM        (10)
SceJumpFixup jumpFixups[INIT_PATCH_GAMES_NUM] = {
    {MEDAL_OF_HONORS_HEROES_GAME_ID_US, g_mohhUsEuStrcpyFixups, 1, g_mohhUsEuWcscpyFixups, 7},
    {MEDAL_OF_HONORS_HEROES_GAME_ID_EU, g_mohhUsEuStrcpyFixups, 1, g_mohhUsEuWcscpyFixups, 1},
    {MEDAL_OF_HONORS_HEROES_GAME_ID_FR, g_mohhUsEuStrcpyFixups, 1, g_mohhUsEuWcscpyFixups, 1},
    {MEDAL_OF_HONORS_HEROES_GAME_ID_DE, g_mohhUsEuStrcpyFixups, 1, g_mohhUsEuWcscpyFixups, 1},
    {MEDAL_OF_HONORS_HEROES_GAME_ID_EU_PLATIN, g_mohhUsEuStrcpyFixups, 1, g_mohhUsEuWcscpyFixups, 1},
    {MEDAL_OF_HONORS_HEROES_GAME_ID_SP, g_mohhUsEuStrcpyFixups, 1, g_mohhUsEuWcscpyFixups, 1},
    {MEDAL_OF_HONORS_HEROES_GAME_ID_NL, g_mohhUsEuStrcpyFixups, 1, g_mohhUsEuWcscpyFixups, 1},
    {MEDAL_OF_HONORS_HEROES_GAME_ID_UK, g_mohhUkKrStrcpyFixups, 1, g_mohhUkKrWcscpyFixups, 1},
    {MEDAL_OF_HONORS_HEROES_GAME_ID_KOR, g_mohhUkKrStrcpyFixups, 1, g_mohhUkKrWcscpyFixups, 1},
    {MEDAL_OF_HONORS_HEROES_GAME_ID_JAP, g_mohhJpStrcpyFixups, 1, g_mohhJpWcscpyFixups, 1}
};

void patchGames(void)
{   
    SceGameInfo *gameInfo;
    s32 lastStrcpyOp, lastWcscpyOp;
    
    gameInfo = sceKernelGetGameInfo();
    lastStrcpyOp = 0;
    lastWcscpyOp = 0;
    
    if (gameInfo == NULL)
        return;
    
    s32 j;
    for (j = 0; j < INIT_PATCH_GAMES_NUM; j++) {
        if (strcmp((char*)gameInfo->sfoProductString, jumpFixups[j].gameId) == 0) {
            s32 i;
            for (i = 0; i < jumpFixups[j].numStrcpy256; i++) {
                s32 *ptr = jumpFixups[j].strcpy256[i];
                if (IS_JUMP(*ptr)) {
                    lastStrcpyOp = JUMP((s32)ptr, *ptr);
                    *ptr = MAKE_JAL(SCE_USERSPACE_ADDR_KU0);
                }
            }
            for (i = 0; i < jumpFixups[j].numWcscpy256; i++) {
                s32 *ptr = jumpFixups[j].wcscpy256[i];
                if (IS_JUMP(*ptr)) {
                    lastWcscpyOp = JUMP((s32)ptr, *ptr);
                    *ptr = MAKE_JAL(SCE_USERSPACE_ADDR_KU0 + (u32)wcscpy256 - (u32)strcpy256);
                }
            }
        }
    }
    if (lastWcscpyOp != 0 || lastStrcpyOp != 0) {
        memcpy((void *)SCE_USERSPACE_ADDR_KU0, strcpy256, (u32)&patchGames - (u32)strcpy256);
    }
    sceKernelDcacheWritebackAll();
}
