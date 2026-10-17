/*
 * first_loader's program, run at 0x5c000 after reset copies it there: it brings up the
 * Cmep's keyrings, reads second_loader from the eMMC (Sdif) or the SD card, decrypts and
 * checks it, and starts it. It uses the Cmep control block (0xe0000000), the keyring
 * controller (0xe0030000), the bignum unit (0xe0040000), Bigmac (0xe0050000), Sdif
 * (0xe0b00000), GPIO 0 (0xe20a0000) and Pervasive (0xe3100000).
 *
 * Skeleton. None of its 74 functions is written yet. The key, signature and
 * decryption routines among them become placeholders (PLACEHOLDER: cryptographic
 * routine, deferred) when they are reached.
 */

#ifndef FL_BUILD_DATE
#error "FL_BUILD_DATE (build, YYYYMMDD in hex) not set"
#endif

/* code that differs between the builds: FL_SINCE(0x20110616) */
#define FL_SINCE(date)	(FL_BUILD_DATE >= (date))
