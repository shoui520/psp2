/*
 * Miniboot: the CP's first stage (ARM1176, 0x1000 bytes), loaded by the EMMA ROM from
 * eMMC partition 1 to SRAM at 0xa0000000. It reads the GPIO seed, checks its CRC,
 * loads NBL (stage 2) from eMMC to DRAM and decrypts it, then jumps there.
 *
 * Skeleton. Not written yet. The stage 2 decryption is a cryptographic routine
 * (PLACEHOLDER: cryptographic routine, deferred).
 */
