/*
 * PlayStation 1 BIOS kernel startup, memory/heap setup, and SYSTEM.CNF / PS-X EXE bootstrap.
 */

#include "bios_internal.h"

static s32 isSpaceChar(char ch)
{
    return (ch == ' ' || ch == '\t' || ch == '\r');
}

static s32 parseHexDigit(char ch)
{
    if (ch >= '0' && ch <= '9')
        return (s32)(ch - '0');
    if (ch >= 'a' && ch <= 'f')
        return (s32)(ch - 'a' + 10);
    if (ch >= 'A' && ch <= 'F')
        return (s32)(ch - 'A' + 10);
    return -1;
}

static const char *findConfigKey(const char *text, const char *key)
{
    size_t keyLen = biosStrlen(key);
    const char *line = text;

    while (*line != '\0') {
        if (biosStrncmp(line, key, keyLen) == 0) {
            const char *cursor = line + keyLen;
            while (isSpaceChar(*cursor))
                cursor++;
            if (*cursor == '=') {
                cursor++;
                while (isSpaceChar(*cursor))
                    cursor++;
                return cursor;
            }
        }
        while (*line != '\0' && *line != '\n')
            line++;
        if (*line == '\n')
            line++;
    }
    return NULL;
}

static u32 parseHexKey(const char *text, const char *key, u32 fallback)
{
    const char *cursor = findConfigKey(text, key);
    u32 value = 0;
    s32 digit;

    if (cursor == NULL)
        return fallback;
    if (cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X'))
        cursor += 2;
    while ((digit = parseHexDigit(*cursor)) >= 0) {
        value = (value << 4) | (u32)digit;
        cursor++;
    }
    return value;
}

static void parseSystemCnf(const char *text)
{
    const char *bootCursor;
    u32 idx = 0;

    PSX_BIOS->cfgTcbCount = parseHexKey(text, "TCB", 4U);
    PSX_BIOS->cfgEvcbCount = parseHexKey(text, "EVENT", 16U);
    PSX_BIOS->cfgStackTop = parseHexKey(text, "STACK", 0x801FFF00U);
    PSX_BIOS->cfgBootPath[0] = '\0';

    bootCursor = findConfigKey(text, "BOOT");
    if (bootCursor != NULL) {
        while (bootCursor[idx] != '\0' && bootCursor[idx] != '\r' &&
               bootCursor[idx] != '\n' && !isSpaceChar(bootCursor[idx]) &&
               idx < sizeof(PSX_BIOS->cfgBootPath) - 1U) {
            PSX_BIOS->cfgBootPath[idx] = bootCursor[idx];
            idx++;
        }
        PSX_BIOS->cfgBootPath[idx] = '\0';
    }
}

void biosInstallVectors(void)
{
    /* Install A0/B0/C0/Brk and general exception vectors in KSEG0 low RAM */
    biosMemcpy((void *)0x80000040U, (const void *)biosBrkVector, 16U);
    biosMemcpy((void *)0x80000080U, (const void *)biosExceptionVector, 16U);
    biosMemcpy((void *)0x800000A0U, (const void *)biosA0Vector, 32U);
    biosMemcpy((void *)0x800000B0U, (const void *)biosB0Vector, 32U);
    biosMemcpy((void *)0x800000C0U, (const void *)biosC0Vector, 32U);
    biosPopulateSyscallTables();
    biosFlushCache();
}

void biosInitKernelTables(u32 tcbCount, u32 evcbCount)
{
    u32 i;

    if (tcbCount == 0U || tcbCount > 16U)
        tcbCount = 4U;
    if (evcbCount == 0U || evcbCount > 64U)
        evcbCount = 16U;

    PSX_BIOS->tcbCount = tcbCount;
    PSX_BIOS->evcbCount = evcbCount;

    biosMemset(PSX_BIOS->tcbs, 0, sizeof(PSX_BIOS->tcbs));
    biosMemset(PSX_BIOS->evcbs, 0, sizeof(PSX_BIOS->evcbs));
    biosMemset(PSX_BIOS->intRpHeads, 0, sizeof(PSX_BIOS->intRpHeads));

    for (i = 0; i < tcbCount; i++)
        PSX_BIOS->tcbs[i].status = TCB_STATUS_FREE;
    PSX_BIOS->tcbs[0].status = TCB_STATUS_ALLOC;
    PSX_BIOS->pcb.currentTcb = &PSX_BIOS->tcbs[0];

    /* Publish standard PS1 kernel table directory at 0x80000100 */
    biosMemset((void *)0x80000100U, 0, 0x80U);
    PSX_KTABLES[PSX_KTABLE_EXCB].base = PSX_BIOS->intRpHeads;
    PSX_KTABLES[PSX_KTABLE_EXCB].byteSize = sizeof(PSX_BIOS->intRpHeads);
    PSX_KTABLES[PSX_KTABLE_PCB].base = &PSX_BIOS->pcb;
    PSX_KTABLES[PSX_KTABLE_PCB].byteSize = sizeof(PSX_BIOS->pcb);
    PSX_KTABLES[PSX_KTABLE_TCB].base = PSX_BIOS->tcbs;
    PSX_KTABLES[PSX_KTABLE_TCB].byteSize = tcbCount * sizeof(PsxTcb);
    PSX_KTABLES[PSX_KTABLE_EVCB].base = PSX_BIOS->evcbs;
    PSX_KTABLES[PSX_KTABLE_EVCB].byteSize = evcbCount * sizeof(PsxEvcb);
    PSX_KTABLES[PSX_KTABLE_FCB].base = PSX_BIOS->fcbs;
    PSX_KTABLES[PSX_KTABLE_FCB].byteSize = sizeof(PSX_BIOS->fcbs);
    PSX_KTABLES[PSX_KTABLE_DCB].base = PSX_BIOS->dcbs;
    PSX_KTABLES[PSX_KTABLE_DCB].byteSize = sizeof(PSX_BIOS->dcbs);
}

void biosSysInitMemory(u32 base, u32 size)
{
    PSX_BIOS->heapBase = (base + 3U) & ~3U;
    PSX_BIOS->heapSize = size & ~3U;
}

void biosInitHeap(void *addr, u32 size)
{
    biosSysInitMemory((u32)addr, size);
}

s32 biosLoadExe(const char *path, PsxExeHeader *hdrOut)
{
    s32 fileDesc;
    s32 readLen;
    PsxExeHeader hdr;

    if (path == NULL || hdrOut == NULL)
        return 0;

    fileDesc = biosFileOpen(path, 1U);
    if (fileDesc < 0)
        return 0;

    readLen = biosFileRead(fileDesc, PSX_BIOS->sectorBuf, 2048U);
    if (readLen < 2048) {
        biosFileClose(fileDesc);
        return 0;
    }

    biosMemcpy(&hdr, PSX_BIOS->sectorBuf, sizeof(hdr));
    if (biosMemcmp(hdr.magic, "PS-X EXE", 8U) != 0) {
        biosFileClose(fileDesc);
        return 0;
    }

    if (hdr.loadSize != 0U) {
        readLen = biosFileRead(fileDesc, (void *)hdr.loadAddr, hdr.loadSize);
        if (readLen < (s32)hdr.loadSize) {
            biosFileClose(fileDesc);
            return 0;
        }
    }
    biosFileClose(fileDesc);

    if (hdr.bssAddr != 0U && hdr.bssSize != 0U)
        biosMemset((void *)hdr.bssAddr, 0, hdr.bssSize);

    *hdrOut = hdr;
    biosFlushCache();
    return 1;
}

s32 biosExecExe(PsxExeHeader *hdr, s32 argc, char **argv)
{
    u32 sp;
    u32 fp;

    if (hdr == NULL || hdr->initialPc == 0U)
        return -1;

    sp = hdr->stackBase + hdr->stackSize;
    if (sp == 0U)
        sp = PSX_BIOS->cfgStackTop ? PSX_BIOS->cfgStackTop : 0x801FFF00U;
    fp = sp;

    (void)argc;
    (void)argv;
    biosFlushCache();
    biosJumpToExe(hdr->initialPc, hdr->initialGp, sp, fp);
    return 0;
}

s32 biosLoadExec(const char *path, u32 stackBase, u32 stackOffset)
{
    PsxExeHeader hdr;

    if (!biosLoadExe(path, &hdr))
        return -1;
    if (stackBase != 0U) {
        hdr.stackBase = stackBase;
        hdr.stackSize = stackOffset;
    }
    return biosExecExe(&hdr, 0, NULL);
}

void biosBootstrap(void)
{
    s32 fileDesc;
    s32 bytesRead;
    const char *bootFile = "cdrom:PSX.EXE;1";
    PsxExeHeader hdr;

    biosResetGpu();
    biosCdInit();

    fileDesc = biosFileOpen("cdrom:SYSTEM.CNF;1", 1U);
    if (fileDesc >= 0) {
        bytesRead = biosFileRead(fileDesc, PSX_BIOS->sectorBuf, 2047U);
        biosFileClose(fileDesc);
        if (bytesRead > 0) {
            PSX_BIOS->sectorBuf[bytesRead] = '\0';
            parseSystemCnf((const char *)PSX_BIOS->sectorBuf);
            if (PSX_BIOS->cfgBootPath[0] != '\0')
                bootFile = PSX_BIOS->cfgBootPath;
        }
    } else {
        PSX_BIOS->cfgTcbCount = 4U;
        PSX_BIOS->cfgEvcbCount = 16U;
        PSX_BIOS->cfgStackTop = 0x801FFF00U;
    }

    biosInitKernelTables(PSX_BIOS->cfgTcbCount, PSX_BIOS->cfgEvcbCount);
    biosInitDevices();
    biosCdRegisterDevice();
    biosCardRegisterDevice();
    biosCdInit();

    /* Run the boot shell copied from 0xBFC18000 (NTSC or PAL overlay) at 0x80030000 */
    biosMemcpy((void *)PSX_RAM_SHELL_ADDR, (const void *)PSX_ROM_SHELL_ADDR, PSX_ROM_SHELL_SIZE);
    biosFlushCache();
    ((void (*)(void))PSX_RAM_SHELL_ADDR)();

    if (biosLoadExe(bootFile, &hdr)) {
        if (hdr.stackBase == 0U) {
            hdr.stackBase = PSX_BIOS->cfgStackTop;
            hdr.stackSize = 0U;
        }
        biosExecExe(&hdr, 0, NULL);
    }

    for (;;)
        ;
}

void biosWarmBoot(void)
{
    biosMainBoot();
}

void biosMainBoot(void)
{
    /* Clear kernel RAM work area and hardware IRQ masks */
    biosMemset((void *)0x80000000U, 0, 0x10000U);
    PSX_IO32(REG_I_MASK) = 0U;
    PSX_IO32(REG_I_STAT) = 0U;
    PSX_IO32(REG_DPCR) = 0x07654321U;
    PSX_IO32(REG_DICR) = 0U;

    /* Standard PlayStation BIOS memory-size descriptor words at 0x0060..0x0068 */
    *(volatile u32 *)0x80000060U = 2U;
    *(volatile u32 *)0x80000064U = 0U;
    *(volatile u32 *)0x80000068U = 0xFFU;

    PSX_BIOS->randSeed = 1U;
    biosInitKernelTables(4U, 16U);
    biosInstallVectors();
    biosInitDevices();
    biosCdRegisterDevice();
    biosCardRegisterDevice();
    biosBootstrap();
}
