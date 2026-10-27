/*
 * kbl_param.c: SceKblParam population in secure SRAM (0x1f000100), IdStorage
 * PsCode / OpenPsId formatting, MBR total-sector lookup, and TRNG / session key
 * generation (0x801b38..0x802043).
 *
 * Uses Sony's SceKblParam and SceMbr definitions from
 * <kernel_boot_loader/kbl_param.h> and <sdstor/mbr.h>.
 * Function names here are ours (descriptive).
 */

#include "second_loader.h"

/* ============================================================================
 * .rodata (0x80bd88..0x80bdc7)
 * ============================================================================ */

static const uint8_t s_trngAesIv1[16] = {
	0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
	0x08u, 0x09u, 0x0au, 0x0cu, 0x0cu, 0x0du, 0x0eu, 0x0fu,
};

static const uint8_t s_trngAesKey1[16] = {
	0xb2u, 0x46u, 0xabu, 0x6au, 0xc5u, 0x02u, 0x09u, 0xd8u,
	0xfdu, 0xaeu, 0xc6u, 0x85u, 0xafu, 0x75u, 0xf5u, 0x54u,
};

static const uint8_t s_trngAesIv2[16] = {
	0x08u, 0x09u, 0x0au, 0x0cu, 0x0cu, 0x0du, 0x0eu, 0x0fu,
	0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
};

static const uint8_t s_trngAesKey2[16] = {
	0xfdu, 0xaeu, 0xc6u, 0x85u, 0xafu, 0x75u, 0xf5u, 0x54u,
	0xb2u, 0x46u, 0xabu, 0x6au, 0xc5u, 0x02u, 0x09u, 0xd8u,
};

/* ============================================================================
 * Functions (0x801b38..0x802043)
 * ============================================================================ */

/* 0x801b38 */
int readIdStoragePsCodeRaw(void *dst16)
{
	return readIdStoragePscodeFromPartition(0x10u, dst16,
			(void *)0x40001500u, 0x1000u);
}

/* 0x801b4c */
uint16_t formatPsCodeFromLeaf(const uint8_t *raw16, ScePsCode *outPsCode)
{
	uint32_t rawFactory;
	uint16_t factoryCode;

	outPsCode->company_code = (uint16_t)(((uint16_t)raw16[3] << 8) | raw16[2]);
	outPsCode->product_code = (uint16_t)(((uint16_t)raw16[5] << 8) | raw16[4]);
	rawFactory = *(const uint32_t *)(const void *)(raw16 + 8);
	outPsCode->product_sub_code = (uint16_t)(((uint16_t)raw16[7] << 8) | raw16[6]);
	factoryCode = (uint16_t)bswap32Mbr(rawFactory);
	outPsCode->factory_code = factoryCode;
	return factoryCode;
}

/* 0x801b98 */
int readIdStorageOpenPsId(void *dst16)
{
	return readIdStorageOpenPsIdFromPartition(0x10u, dst16,
			(void *)0x40001500u, 0x1000u);
}

/* 0x801bac */
int readEmmcMbrTotalSectors(uint32_t *outSectors)
{
	uint8_t smiLeaf[512];
	int status;

	*outSectors = 0xffffffffu;
	status = readIdStorageSmiLeaf80(smiLeaf, sizeof(smiLeaf),
			(void *)0x40001500u, 0x1000u);
	if (status == 0)
		*outSectors = *(const uint32_t *)(const void *)(smiLeaf + 0x80u);
	return status;
}

/* 0x801c00 */
uint32_t readBigmacRngWord(void)
{
	return BIGMAC->ch[0].rng;
}

/* 0x801c0c */
int clearKblParam(void)
{
	return bigmacMemset((void *)(uintptr_t)KBL_PARAM_PA, sizeof(SceKblParam), 0);
}

/* 0x801c1e */
int restoreWarmBootKblParam(void)
{
	return bigmacMemcpy((void *)(uintptr_t)KBL_PARAM_PA,
			(const void *)(uintptr_t)KBL_PARAM_WARMBOOT_PA,
			sizeof(SceKblParam));
}

/* 0x801c36 */
int populateKblParam(const uint32_t *dipsw, const uint8_t *qaFlags,
		uint32_t wakeupFactor, int isWarmBoot, uint32_t minRequiredFwVer)
{
	uint32_t rawRng[8];
	uint8_t encryptedRng[32];
	uint32_t krBootFlags;
	uint32_t dramSize;
	uint8_t iv[16];
	unsigned int i;

	KBL_PARAM->wakeup_factor = wakeupFactor;
	slMemcpy(KBL_PARAM->dipswWords, dipsw, sizeof(KBL_PARAM->dipswWords));
	slMemcpy(KBL_PARAM->qa_flags, qaFlags, sizeof(KBL_PARAM->qa_flags));

	dramSize = ddrcGetTotalDramSize();
	KBL_PARAM->version = 1u;
	KBL_PARAM->size = sizeof(SceKblParam);
	KBL_PARAM->dram.addr = 0x40000000u;
	KBL_PARAM->dram.size = dramSize;
	writeKeyringSlotBytes(0x513u, &dramSize, sizeof(dramSize));

	krBootFlags = 0u;
	KBL_PARAM->boot_type_indicator_1 = 0u;
	if (isBootFlagBit0Enabled()) {
		KBL_PARAM->boot_type_indicator_1 |= 0x00000001u;
		krBootFlags |= 0x00000004u;
	}
	if (isKernelBootLoaderDebugMode())
		KBL_PARAM->boot_type_indicator_1 |= 0x00000002u;
	if (isSnvsSd0FlagEnabled())
		KBL_PARAM->boot_type_indicator_1 |= 0x00000004u;
	if (isSnvsBit1FlagEnabled())
		KBL_PARAM->boot_type_indicator_1 |= 0x00080000u;
	if (isBootFlagBit3KblEnabled())
		KBL_PARAM->boot_type_indicator_1 |= 0x00000008u;
	if (isBootFlagBit16Enabled()) {
		KBL_PARAM->boot_type_indicator_1 |= 0x00010000u;
		krBootFlags |= 0x00000020u;
	}
	if (isWarmBoot != 0) {
		KBL_PARAM->boot_type_indicator_1 |= 0x00020000u;
		KBL_PARAM->kbl_load_time = 0u;
		krBootFlags |= 0x00000100u;
	}
	if (isSd0PowerEnabled())
		KBL_PARAM->boot_type_indicator_1 |= 0x00040000u;
	if (isBootFlagBit3Enabled())
		krBootFlags |= 0x00000008u;

	KBL_PARAM->boot_flags[2] = (uint8_t)getHwFlagByte4();
	KBL_PARAM->boot_flags[1] = (uint8_t)getNvsBootFlagByte1();
	KBL_PARAM->boot_flags[3] = (uint8_t)getNvsBootFlagByte3();
	KBL_PARAM->boot_flags[4] = (uint8_t)getNvsBootFlagByte7();
	KBL_PARAM->boot_flags[5] = (uint8_t)getNvsBootFlagByte6();

	writeKeyringSlotBytes(0x50cu, &krBootFlags, sizeof(krBootFlags));
	KBL_PARAM->boot_type_indicator_1 |= (getBootModulesFlags() << 4);

	KBL_PARAM->secure_kernel.addr = getBootModuleLoadAddr(0);
	KBL_PARAM->secure_kernel.size = getBootModuleLoadSize(0);
	KBL_PARAM->context_auth_sm.addr = getBootModuleLoadAddr(1);
	KBL_PARAM->context_auth_sm.size = getBootModuleLoadSize(1);
	KBL_PARAM->kprx_auth_sm.addr = getBootModuleLoadAddr(2);
	KBL_PARAM->kprx_auth_sm.size = getBootModuleLoadSize(2);
	KBL_PARAM->prog_rvk.addr = getBootModuleLoadAddr(3);
	KBL_PARAM->prog_rvk.size = getBootModuleLoadSize(3);

	KBL_PARAM->bootldr_revision = g_bootldrRevision;
	KBL_PARAM->current_fw_version = 0x04000000u;
	KBL_PARAM->factory_fw_version = minRequiredFwVer;
	KBL_PARAM->__stack_chk_guard = readBigmacRngWord();
	KBL_PARAM->reserved3 = readBigmacRngWord();

	if (isWarmBoot != 0) {
		const SceKblParam *warmKbl = (const SceKblParam *)(uintptr_t)KBL_PARAM_WARMBOOT_PA;

		KBL_PARAM->dipswWords[3] = warmKbl->dipswWords[3];
	} else {
		if (!isDevkitFeatureEnabled() ||
		    !checkDipswBit(dipsw, 0xd6u) ||
		    !checkBitFlagInArray(qaFlags, 14, 1u)) {
			KBL_PARAM->dipswWords[3] = readBigmacRngWord();
		}
		bigmacMemcpy((void *)(uintptr_t)KBL_PARAM_WARMBOOT_PA,
				(const void *)(uintptr_t)KBL_PARAM_PA,
				sizeof(SceKblParam));
	}

	/* Populate 16-byte KBL_PARAM->session_id and encrypt in place with AES-128-CBC */
	*(uint32_t *)&KBL_PARAM->session_id[0] = KBL_PARAM->dipswWords[3];
	*(uint32_t *)&KBL_PARAM->session_id[4] = readBigmacRngWord();
	*(uint32_t *)&KBL_PARAM->session_id[8] = readBigmacRngWord();
	*(uint32_t *)&KBL_PARAM->session_id[12] = readBigmacRngWord();

	slMemcpy(iv, s_trngAesIv1, sizeof(iv));
	bigmacAes128CbcEncryptSync(KBL_PARAM->session_id, KBL_PARAM->session_id,
			16u, s_trngAesKey1, iv);

	/* Populate 32-byte keyring slot 0x51a and KBL_PARAM->coredump_session_key (0x1f000200) */
	slMemcpy(iv, s_trngAesIv2, sizeof(iv));
	for (i = 0; i < 8u; i++)
		rawRng[i] = readBigmacRngWord();

	KEYRING_CTRL->data = rawRng[0];
	KEYRING_CTRL->reserved0[0] = rawRng[1];
	KEYRING_CTRL->reserved0[1] = rawRng[2];
	KEYRING_CTRL->reserved0[2] = rawRng[3];
	KEYRING_CTRL->reserved0[3] = rawRng[4];
	KEYRING_CTRL->reserved0[4] = rawRng[5];
	KEYRING_CTRL->reserved0[5] = rawRng[6];
	KEYRING_CTRL->reserved0[6] = rawRng[7];
	KEYRING_CTRL->protection = 0x51au;

	bigmacAes128CbcEncryptSync(encryptedRng, rawRng, sizeof(rawRng), s_trngAesKey2, iv);
	slMemcpy((void *)(uintptr_t)(KBL_PARAM_PA + 0x100u), encryptedRng, sizeof(encryptedRng));
	return 0;
}

/* 0x801fc0 */
void populateSysconKblFields(void)
{
	sceSysconReadScratchpad(0x0cu, &KBL_PARAM->suspendinfo_adr, sizeof(KBL_PARAM->suspendinfo_adr));
	sceSysconGetPoweroffFactor(&KBL_PARAM->usb_info);
	sceSysconGetUsbDetStatus(&KBL_PARAM->hold_ctrl);
	sceSysconGetPowerInfo(&KBL_PARAM->poweroff_factor);
	KBL_PARAM->boot_type_indicator_2 = (uint32_t)sceSysconGetBaryonTimestamp();
	KBL_PARAM->hardware_info = (uint32_t)sceSysconGetHardwareInfo();
	if (sceSysconReadBootSummary(KBL_PARAM->hardware_flags) < 0)
		slMemset(KBL_PARAM->hardware_flags, 0, sizeof(KBL_PARAM->hardware_flags));
}
