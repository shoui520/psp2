/*
 * PlayStation Vita gamecard authentication secure module (os0:sm/gcauthmgr_sm.self)
 * Reconstructed from MeP C5 binary (0x80b000..0x813308).
 */

#include <stddef.h>
#ifdef SM_HOST_TEST
#include <stdint.h>
#else
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef unsigned long uintptr_t;
#endif
#include "sm.h"

#ifndef SM_HOST_TEST
#define MMIO_PTR(addr)          ((volatile uint32_t *)(uintptr_t)(addr))
#define MMIO_U8_PTR(addr)       ((volatile uint8_t *)(uintptr_t)(addr))
#define MMIO_READ32(addr)       (*MMIO_PTR(addr))
#define MMIO_WRITE32(addr, val) (*MMIO_PTR(addr) = (uint32_t)(val))
#define MMIO_READ8(addr)        (*MMIO_U8_PTR(addr))
#define MMIO_ADDPTR(addr)       ((uintptr_t)(addr))
#define MMIO_RAW_PTR(addr)      ((void *)(uintptr_t)(addr))

static inline void cmep_synm(void)
{
	__asm__ volatile("syncm" ::: "memory");
}

static inline uint32_t cmep_syscall(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3)
{
	register uint32_t r0 __asm__("$0");
	register uint32_t r1 __asm__("$1") = a1;
	register uint32_t r2 __asm__("$2") = a2;
	register uint32_t r3 __asm__("$3") = a3;
	register uint32_t r4 __asm__("$4") = num;

	__asm__ volatile("swi 0" : "=r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r4) : "memory");
	return r0;
}

static inline void cmep_stcb(uint32_t reg, uint32_t val)
{
	switch (reg) {
	case 0x410:
		__asm__ volatile("stcb %0, 0x410" :: "r"(val));
		break;
	case 0x411:
		__asm__ volatile("stcb %0, 0x411" :: "r"(val));
		break;
	case 0x412:
		__asm__ volatile("stcb %0, 0x412" :: "r"(val));
		break;
	case 0x413:
		__asm__ volatile("stcb %0, 0x413" :: "r"(val));
		break;
	case 0x415:
		__asm__ volatile("stcb %0, 0x415" :: "r"(val));
		break;
	}
}

static inline uint32_t cmep_ldcb(uint32_t reg)
{
	uint32_t val = 0;

	switch (reg) {
	case 0x410:
		__asm__ volatile("ldcb %0, 0x410" : "=r"(val));
		break;
	case 0x412:
		__asm__ volatile("ldcb %0, 0x412" : "=r"(val));
		break;
	case 0x415:
		__asm__ volatile("ldcb %0, 0x415" : "=r"(val));
		break;
	}
	return val;
}
#endif

/* Forward declarations */
void sm_stack_chk_fail(void);
int gcauthmgr_main(void);
void sm_service_invoke_cb(void *self, void *req);
void keyring_read_slot(uint32_t slot, uint32_t *dst);
int kirk_validate_req_size(const void *req);
int kirk_cmd_0d_ec_mul_160(void *req);
int kirk_cmd_11_ecdsa_verify_160(void *req);
uint32_t bigmac_derive_keyslot_ecb(const void *key_src, uint32_t dst_slot, uint32_t src_slot);
uint32_t bigmac_derive_keyslot_cbc(const void *key_src, const void *iv_src, uint32_t dst_slot, uint32_t src_slot);
uint32_t bigmac_aes_cbc_keyslot(void *dst, const void *src, uint32_t len, uint32_t keyslot, uint32_t mode);
uint32_t bigmac_load_keyslot_204(const void *key_src);
uint32_t bigmac_aes_ecb_slot0(void *dst, const void *src, uint32_t len);
int kirk_cmd_15_ec_mul_224(void *req);
int kirk_cmd_18_ecdsa_verify_224(void *req);
uint32_t bigmac_cmac_keyslot(void *dst, const void *src, uint32_t len, uint32_t keyslot);
int bigmac_wait_done(uint32_t *ctx);
int kirk_aes_cbc_encrypt(void *buf, uint32_t len, const void *key, int mode);
int kirk_aes_cbc_decrypt(void *buf, uint32_t len, const void *key, int mode);
int kirk_aes_cmac(const void *src, uint32_t len, const void *key, void *dst_mac);
int kirk_sha256(const void *src, uint32_t len, void *dst_hash);
int kirk_sha1(const void *src, uint32_t len, void *dst_hash);
int kirk_derive_session_key(const void *seed, uint32_t key_id, void *out_key, uint32_t *out_mode);
int kirk_cmd_1b_verify_challenge(void *req);
int kirk_decrypt_session_key(const void *seed, uint32_t key_id, const void *enc_in, void *dec_out);
int kirk_cmd_20_verify_key_blocks(void *req);
int kirk_cmd_1f_decrypt_session_block(void *req);
int kirk_cmd_1d_verify_cert_block(void *req);
int kirk_cmd_23_encrypt_and_cmac(void *req);
int kirk_hmac_sha256_init_state(const void *part_a, const void *part_b, uint32_t part_len, void *out_digest);
int kirk_hmac_sha256_drbg_step(const uint8_t *seed, uint32_t seed_len, uint8_t *state, uint8_t *out_block);
int kirk_cmd_22_ecdsa_sign_224_det(void *req);
int kirk_cmd_21_ecdsa_sign_160_det(void *req);
int kirk_rng_hmac_sha256(void *dst, uint32_t len);
int kirk_cmd_1e_gen_challenge_b1(void *req);
int kirk_cmd_1c_gen_token_a3(void *req);
int kirk_cmd_17_ecdsa_sign_224(void *req);
int kirk_cmd_16_rng_28(void *req);
int kirk_cmd_14_ecdsa_keygen_224(void *req);
int kirk_cmd_10_ecdsa_sign_160(void *req);
int kirk_cmd_0e_rng_20(void *req);
int kirk_cmd_0c_ecdsa_keygen_160(void *req);
int kirk_derive_key_509_50d(void *out_key);
int kirk_cmd_07_decrypt_key100(void *req);
int kirk_cmd_07_aes_cbc_decrypt(void *req);
int kirk_cmd_04_encrypt_key100(void *req);
int kirk_cmd_04_aes_cbc_encrypt(void *req);
uint32_t bigmac_aes_ecb_encrypt_slot0(void *dst, const void *src);
int kirk_cmac_slot0_software(uint8_t *mac_out, const uint8_t *msg, uint32_t msg_len);
int kirk_cmd_19_verify_cmac_d8(void *req);
int kirk_cmd_12_verify_cmac_a8(void *req);
void keyring_read_slot_509(void *dst);
void keyring_read_slot_50d(void *dst);
void keyring_read_slot_600_rev(uint8_t *dst);
int kirk_cmd_24_gc_build_auth_pkt(void *req);
int kirk_cmd_25_gc_verify_auth_resp(void *req);
void kirk_service_dispatch(void *pkt);
int sm_validate_phys_range(uint32_t addr, uint32_t size);
void sm_mailbox_irq_handler(void);
int sm_srv_init_zero(uintptr_t *ctx);
void sm_srv_init(uintptr_t *ctx);
uintptr_t *sm_srv_get_ctx(void);
uintptr_t *sm_srv_alloc_node(uintptr_t *ctx);
int sm_srv_free_node(uintptr_t *ctx, uintptr_t *node);
void sm_srv_teardown(uintptr_t *ctx);
void sm_srv_teardown_all(uintptr_t *ctx);
void sm_srv_run_loop(uintptr_t *ctx);
uint32_t sm_srv_unlink_node(uintptr_t *ctx, uintptr_t *node);
void sm_mem_barrier(void);
int bigmac_ctx_init_ch0(uint32_t *ctx);
int bigmac_ctx_get_status(const uint32_t *ctx, uint32_t *status_out);
uint32_t bigmac_ctx_wait(const uint32_t *ctx);
uint32_t bigmac_aes128_cbc_encrypt(void *dst, const void *src, uint32_t len, const void *key, const void *iv, uint32_t *ctx);
uint32_t bigmac_aes128_cbc_decrypt(void *dst, const void *src, uint32_t len, const void *key, const void *iv, uint32_t *ctx);
uint32_t bigmac_sha1_start(void *dst, const void *src, uint32_t len, uint32_t *ctx);
uint32_t bigmac_sha256_start(void *dst, const void *src, uint32_t len, uint32_t *ctx);
uint32_t bigmac_hmac_sha256_start(void *dst, const void *src, uint32_t len, const void *key, uint32_t key_len, uint32_t *ctx);
uint32_t bigmac_aes128_cmac_inline(void *dst, const void *src, uint32_t len, const void *key, uint32_t key_len, uint32_t *ctx);
uint32_t bigmac_dma_memcpy(void *dst, const void *src, uint32_t len);
uint32_t bigmac_cipher_start(void *dst, const void *src, uint32_t len, const void *key, const void *iv,
		    uint32_t op, uint32_t mode, uint32_t key_idx, uint32_t *ctx);
uint32_t bigmac_hash_start(void *dst, const void *src, uint32_t len, uint32_t hash_mode, uint32_t *ctx);
uint32_t bigmac_hmac_start(void *dst, const void *src, uint32_t len, const void *key, uint32_t key_len,
		    uint32_t hmac_mode, uint32_t *ctx);
uint32_t bigmac_memcpy_start(void *dst, const void *src, uint32_t len, uint32_t *ctx);
uint32_t sm_syscall_unload(uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t sm_syscall_2(uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t sm_syscall_3(uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t sm_syscall_register_irq(uint32_t a1, void (*a2)(void), uint32_t *a3);
uint32_t sm_syscall_5(uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t sm_syscall_6(uint32_t a1, uint32_t a2, uint32_t a3);
uint32_t sm_syscall_panic(uint32_t a1);
uint32_t sm_syscall_8(uint32_t a1, uint32_t a2, uint32_t a3);
void cop_rng_init(void);
void cop_rng_wait(void);
uint32_t cop_rng_read(void);
uint32_t cop_rng_get_word(void);
int ec_pubkey_from_priv(uint8_t **pub_xy, const uint8_t *priv, const uint8_t * const *curve,
	       uint32_t nwords, uint32_t nbytes);
int ec_pubkey_from_priv_160(uint8_t **pub_xy, const uint8_t *priv, const uint8_t * const *curve);
int ec_pubkey_from_priv_224(uint8_t **pub_xy, const uint8_t *priv, const uint8_t * const *curve);
int bignum_is_valid_scalar(const uint32_t *val, const uint32_t *limit, int nwords);
void ec_scalar_mul_montgomery(uint32_t **out_xy, uint32_t **in_xy, const uint32_t *p, const uint32_t *a,
		const uint32_t *scalar, uint32_t nwords);
int ec_point_mul(uint8_t **out_xy, const uint8_t *scalar, uint8_t **in_xy,
	       const uint8_t * const *curve, uint32_t nwords, uint32_t nbytes);
int ec_point_mul_160(uint8_t **out_xy, const uint8_t *scalar, uint8_t **in_xy,
	       const uint8_t * const *curve);
int ec_point_mul_224(uint8_t **out_xy, const uint8_t *scalar, uint8_t **in_xy,
	       const uint8_t * const *curve);
uint32_t bignum_exec_op_a(uint32_t op, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t *status_out);
int ecdsa_sign_core(uint8_t **sig_rs, const uint8_t *hash, const uint8_t *nonce, const uint8_t *priv,
	       const uint8_t * const *curve, uint32_t nwords, uint32_t nbytes);
int ecdsa_sign_160(uint8_t **sig_rs, const uint8_t *hash, const uint8_t *nonce, const uint8_t *priv,
	       const uint8_t * const *curve);
int ecdsa_sign_224(uint8_t **sig_rs, const uint8_t *hash, const uint8_t *nonce, const uint8_t *priv,
	       const uint8_t * const *curve);
uint32_t bignum_exec_op_b(uint32_t op, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t *status_out);
int ecdsa_verify_core(uint8_t **pub_xy, const uint8_t *hash, uint8_t **sig_rs,
	       const uint8_t * const *curve, uint32_t nwords, uint32_t nbytes);
int ecdsa_verify_160(uint8_t **pub_xy, const uint8_t *hash, uint8_t **sig_rs,
	       const uint8_t * const *curve);
int ecdsa_verify_224(uint8_t **pub_xy, const uint8_t *hash, uint8_t **sig_rs,
	       const uint8_t * const *curve);
int bignum_mod_reduce_scalar(uint8_t *out, const uint8_t *in, uint32_t in_words, uint32_t in_bytes,
	       const uint8_t *mod, uint32_t mod_words, uint32_t mod_bytes);
int bignum_mod_reduce_160(uint8_t *out, const uint8_t *in, const uint8_t *mod);
int bignum_mod_reduce_224(uint8_t *out, const uint8_t *in, const uint8_t *mod);
int bignum_cmp_gt(const uint32_t *a, const uint32_t *b, int nwords);
void bignum_shl_words(uint32_t *dst, const uint32_t *src, int nwords, uint32_t shift);
void bignum_be_to_le_words(uint32_t *dst_le, const uint8_t *src_be, int nbytes);
void bignum_le_words_to_be(uint8_t *dst_be, const uint32_t *src_le, int nwords);
void ec_jacobian_to_affine_step(volatile uint32_t *pt_regs, uint32_t reg_idx, uint32_t nwords);
void ec_point_add_jacobian(uint32_t nwords);
void ec_shamir_dual_scalar_mul(uint32_t **out_xy, uint32_t **p1_xy, uint32_t **p2_xy,
		const uint32_t *s1, const uint32_t *s2,
		const uint32_t *p, const uint32_t *a, uint32_t nwords);
void sm_halt_loop(void);
void *sm_memset(void *dst, int val, uint32_t len);
void *sm_memcpy(void *dst, const void *src, uint32_t len);
uint8_t sm_memcmp_consttime(const void *a, const void *b, int len);
int32_t sm_mulsi3(uint32_t a, int32_t b);
uint32_t sm_udivmodsi4(uint32_t num, uint32_t den, int want_rem);
int32_t sm_divsi3(int32_t a, int32_t b);
int32_t sm_modsi3(int32_t a, int32_t b);
uint32_t sm_udivsi3(uint32_t a, uint32_t b);
uint32_t sm_umodsi3(uint32_t a, uint32_t b);

/* .rodata constant tables (0x812708..0x813300) */
static void (* const g_kirk_service_vtable[2])(void *, void *) = { sm_service_invoke_cb, NULL };

static const uint8_t g_keyseed_kirk_13_24_25[0x10] = {
	0x78, 0xea, 0x6e, 0xd4, 0x3a, 0xb5, 0x59, 0xd4,
	0x15, 0x2a, 0x46, 0x15, 0xf2, 0xd7, 0x4f, 0xc3
};
static const uint8_t g_gc_resp_iv_prefix[0x10] = {
	0x6b, 0x03, 0x4b, 0x05, 0x01, 0xe2, 0x71, 0x4a,
	0xd6, 0xe6, 0x08, 0xe2, 0xcf, 0xa5, 0xee, 0x63
};
static const uint8_t g_keyseed_kirk_68_24[0x10] = {
	0x9c, 0x33, 0x84, 0x3c, 0x60, 0xd6, 0xee, 0x3e,
	0xd4, 0x4e, 0x9e, 0xc0, 0x94, 0xf5, 0xf5, 0x42
};
static const uint8_t g_gc_req_iv_prefix[0x10] = {
	0xcd, 0xd1, 0x80, 0xd9, 0x66, 0x81, 0xbd, 0x40,
	0x31, 0xf5, 0xa9, 0x77, 0xba, 0x63, 0xf1, 0x0e
};
static const uint8_t g_rng_hmac_key[0x40] = {
	0x6d, 0x0b, 0xcd, 0x43, 0xf3, 0x19, 0xba, 0x64,
	0xb4, 0xf1, 0xa6, 0xed, 0x31, 0xd5, 0xd4, 0x16,
	0xc0, 0x11, 0xfc, 0xfc, 0x76, 0x9a, 0xe5, 0x6c,
	0x71, 0x30, 0x24, 0x5c, 0x58, 0xe9, 0x42, 0x56,
	0x14, 0x40, 0x54, 0x4b, 0xc4, 0x76, 0x3c, 0x21,
	0x62, 0xac, 0xda, 0x57, 0x94, 0x22, 0xec, 0xbf,
	0x70, 0x99, 0xe9, 0xd2, 0xdb, 0xbd, 0x98, 0x8d,
	0x08, 0xaa, 0x53, 0xaf, 0x88, 0x09, 0x5c, 0x9f
};
static const uint8_t g_drbg_hmac_init_key[0x40] = {
	0x54, 0x88, 0xa9, 0x81, 0x1c, 0x9a, 0x2c, 0xbc,
	0xcc, 0x59, 0x6b, 0x1f, 0xad, 0x1a, 0x7e, 0x29,
	0xe0, 0x75, 0x84, 0x0f, 0x47, 0x43, 0x1f, 0x37,
	0xac, 0x06, 0x02, 0x46, 0x4a, 0x27, 0x9e, 0x02,
	0xdf, 0x2e, 0x71, 0x65, 0xf1, 0x13, 0x7b, 0xf6,
	0x9a, 0xe6, 0xdc, 0xb9, 0xdc, 0x38, 0x8c, 0x9d,
	0xcc, 0xb3, 0x64, 0xc4, 0xca, 0x26, 0xcb, 0x8f,
	0x1a, 0xf0, 0x63, 0x8a, 0x6e, 0xad, 0xb5, 0x4d
};
static const uint8_t g_key_kirk23_enc[0x10] = {
	0x61, 0x35, 0x87, 0xcc, 0xf0, 0x41, 0xde, 0x6e,
	0xb9, 0x73, 0x97, 0xc2, 0x05, 0xdf, 0x49, 0xe9
};
static const uint8_t g_key_kirk23_cmac[0x10] = {
	0x66, 0x01, 0x54, 0x5f, 0x5e, 0x9d, 0x4d, 0xd8,
	0xf8, 0xed, 0x3b, 0x33, 0x39, 0x26, 0xa8, 0x99
};
static const uint8_t g_keyseed_session_8001[0x10] = {
	0x6f, 0x22, 0x85, 0xed, 0x46, 0x3a, 0x6e, 0x57,
	0xc5, 0xf3, 0x55, 0x0d, 0xdc, 0xc8, 0x1f, 0xeb
};
static const uint8_t g_keyseed_session_8002[0x10] = {
	0xda, 0x96, 0x08, 0xb5, 0x28, 0x82, 0x5d, 0x6d,
	0x13, 0xa7, 0xaf, 0x14, 0x46, 0xb8, 0xec, 0x08
};
static const uint8_t g_keyseed_session_8003[0x10] = {
	0x36, 0x8b, 0x2e, 0xb5, 0x43, 0x7a, 0x82, 0x18,
	0x62, 0xa6, 0xc9, 0x55, 0x96, 0xd8, 0xc1, 0x35
};
static const uint8_t g_keyseed_session_0001[0x10] = {
	0x7f, 0x1f, 0xd0, 0x65, 0xdd, 0x2f, 0x40, 0xb3,
	0xe2, 0x65, 0x79, 0xa6, 0x39, 0x0b, 0x61, 0x6d
};
static const uint8_t g_ivseed_session_0001[0x10] = {
	0x8b, 0x14, 0xc8, 0xa1, 0xe9, 0x6f, 0x30, 0xa7,
	0xf1, 0x01, 0xa9, 0x6a, 0x30, 0x33, 0xc5, 0x5b
};
static const uint8_t g_keyseed_kirk_12_19[0x10] = {
	0x82, 0x1c, 0x57, 0x14, 0x41, 0x5e, 0x98, 0x04,
	0xd6, 0xaa, 0xe3, 0x24, 0xeb, 0x3d, 0xdd, 0xfe
};
static const uint8_t g_ec224_privkey_0[0x1c] = {
	0x76, 0x74, 0x36, 0xa6, 0x99, 0x9d, 0x88, 0x48,
	0x0e, 0xc8, 0x56, 0xf5, 0x5c, 0xea, 0xbb, 0x43,
	0x96, 0x85, 0x9e, 0x37, 0x45, 0x99, 0x40, 0x39,
	0x21, 0xf5, 0x55, 0x98
};
static const uint8_t g_ec224_privkey_1[0x1c] = {
	0x60, 0x7a, 0x2e, 0x55, 0x68, 0xb4, 0xb9, 0xa0,
	0x32, 0xf4, 0x52, 0x53, 0xcf, 0xed, 0x20, 0xdb,
	0x2e, 0x6e, 0x44, 0x6c, 0x37, 0x82, 0xe8, 0x2a,
	0x1a, 0xb9, 0xc9, 0x23
};
static const uint8_t g_keyseed_kirk_10_17[0x10] = {
	0xb0, 0x74, 0x4f, 0x09, 0x1c, 0xd3, 0x33, 0xf9,
	0xf6, 0xa5, 0x24, 0xf4, 0x26, 0xcc, 0x0d, 0x6e
};
static const uint8_t g_ec160_privkey[0x14] = {
	0x53, 0xcc, 0xc3, 0x6e, 0xdf, 0xad, 0xbe, 0x24,
	0x55, 0x83, 0x27, 0x05, 0x52, 0xd2, 0x3b, 0x22,
	0x51, 0x8e, 0xe3, 0xa8
};
static const uint8_t g_keyseed_kirk_02[0x10] = {
	0xf8, 0x1e, 0x0e, 0x97, 0xad, 0xed, 0x52, 0xb3,
	0xa9, 0xfb, 0x83, 0x22, 0xcf, 0x58, 0x92, 0x63
};
static const uint8_t g_keyseed_kirk_03[0x10] = {
	0xa7, 0x01, 0x56, 0x5f, 0x1e, 0xad, 0xdc, 0x05,
	0x17, 0x96, 0xdc, 0x2d, 0x46, 0xdc, 0x50, 0x9a
};
static const uint8_t g_keyseed_kirk_04[0x10] = {
	0xcc, 0xb8, 0x15, 0xdc, 0x41, 0x94, 0xb7, 0xcb,
	0x3f, 0x33, 0xda, 0x7c, 0x1d, 0x53, 0x3e, 0xe5
};
static const uint8_t g_keyseed_kirk_05[0x10] = {
	0x35, 0x5d, 0x76, 0x19, 0x6f, 0x58, 0xda, 0x2b,
	0x90, 0xab, 0x65, 0xb2, 0x7b, 0xdf, 0xe9, 0x2e
};
static const uint8_t g_keyseed_kirk_0c[0x10] = {
	0x9e, 0x6c, 0x4f, 0x1e, 0xbe, 0xd9, 0xa4, 0xcc,
	0x2f, 0xc7, 0x49, 0xcb, 0x40, 0x38, 0x40, 0xc9
};
static const uint8_t g_keyseed_kirk_0d[0x10] = {
	0x6e, 0xc2, 0xf2, 0x55, 0x1d, 0xa4, 0x2b, 0xc6,
	0x8a, 0xab, 0x44, 0x73, 0x90, 0xf1, 0x4c, 0x87
};
static const uint8_t g_keyseed_kirk_0e[0x10] = {
	0xd4, 0x28, 0x2a, 0x71, 0x66, 0x2c, 0x76, 0x6c,
	0x84, 0x0b, 0x0b, 0xc3, 0x61, 0x92, 0x71, 0x68
};
static const uint8_t g_keyseed_kirk_0f[0x10] = {
	0x65, 0x17, 0x5e, 0xcf, 0x5f, 0xef, 0x05, 0xa7,
	0x59, 0x79, 0x8e, 0x96, 0x12, 0xeb, 0x55, 0xad
};
static const uint8_t g_keyseed_kirk_10[0x10] = {
	0x5c, 0x91, 0x4e, 0xd0, 0xf1, 0x8c, 0x1d, 0x2c,
	0x52, 0x8c, 0x1a, 0xd2, 0xb6, 0xe8, 0xec, 0x3e
};
static const uint8_t g_keyseed_kirk_11[0x10] = {
	0x90, 0xef, 0x05, 0x35, 0x9b, 0xbf, 0x56, 0xa2,
	0x34, 0xb0, 0x21, 0x2b, 0x61, 0xb7, 0x02, 0x99
};
static const uint8_t g_keyseed_kirk_12[0x10] = {
	0x2b, 0x35, 0x2a, 0xc0, 0xae, 0xea, 0x17, 0x5b,
	0xd0, 0x27, 0x19, 0x60, 0xb8, 0xa1, 0x86, 0x7f
};
static const uint8_t g_keyseed_kirk_38[0x10] = {
	0x04, 0x72, 0x84, 0xe1, 0x05, 0xee, 0xd5, 0xcc,
	0x61, 0x93, 0x26, 0xf0, 0x25, 0x8e, 0xd5, 0x74
};
static const uint8_t g_keyseed_kirk_39[0x10] = {
	0x40, 0x9c, 0x89, 0x90, 0x23, 0xbc, 0x00, 0xcb,
	0xc1, 0xff, 0x90, 0xd3, 0x18, 0x58, 0xcd, 0x93
};
static const uint8_t g_keyseed_kirk_3a[0x10] = {
	0xfc, 0xf6, 0x84, 0x1f, 0x0e, 0x73, 0xd7, 0xec,
	0x59, 0xb1, 0x12, 0x5d, 0x5a, 0x12, 0x1d, 0x0c
};
static const uint8_t g_keyseed_kirk_44[0x10] = {
	0x3f, 0xb3, 0x00, 0x74, 0x43, 0x1a, 0x5d, 0x39,
	0x16, 0xfd, 0x0e, 0xfa, 0xee, 0xec, 0xa8, 0x0f
};
static const uint8_t g_keyseed_kirk_53[0x10] = {
	0xa9, 0x4c, 0xd0, 0x91, 0x6a, 0xa9, 0x70, 0xbb,
	0x94, 0xf8, 0xc0, 0xa4, 0x11, 0xbb, 0x5c, 0x75
};
static const uint8_t g_keyseed_kirk_57[0x10] = {
	0xfe, 0xb8, 0x0a, 0x86, 0x27, 0xbd, 0x66, 0x9a,
	0x49, 0x82, 0xe1, 0xe4, 0x9f, 0x48, 0xb7, 0x33
};
static const uint8_t g_keyseed_kirk_63[0x10] = {
	0x45, 0xb1, 0x88, 0x16, 0xef, 0x5e, 0xd6, 0x50,
	0xf5, 0xed, 0x7b, 0xf3, 0x27, 0x5a, 0x78, 0x92
};
static const uint8_t g_keyseed_kirk_64[0x10] = {
	0xd4, 0x9c, 0x99, 0x9f, 0x7b, 0x4a, 0xd6, 0x1f,
	0x72, 0x92, 0xfe, 0x6b, 0xf0, 0x56, 0x75, 0x08
};
static const uint8_t g_keyseed_kirk_80[0x10] = {
	0xc8, 0xed, 0x1e, 0xe4, 0xbf, 0xdd, 0xdd, 0x94,
	0x0e, 0xbb, 0xee, 0x85, 0x11, 0xdc, 0x2a, 0x20
};
static const uint8_t g_keyseed_kirk_81[0x10] = {
	0x1a, 0x39, 0xfa, 0x7e, 0x64, 0x3a, 0xdb, 0xee,
	0xa9, 0x83, 0xeb, 0xe8, 0x31, 0x7c, 0x15, 0xce
};
static const uint8_t g_keyseed_kirk_82[0x10] = {
	0x17, 0x3d, 0xf1, 0x51, 0xf8, 0x92, 0x8b, 0x1c,
	0x51, 0x9f, 0x4a, 0x76, 0x23, 0xe1, 0xd3, 0x44
};
static const uint8_t g_keyseed_kirk_83[0x10] = {
	0x27, 0x2c, 0xee, 0xf4, 0x61, 0xc9, 0xd7, 0xab,
	0xa7, 0xf3, 0x4b, 0x4b, 0x2a, 0x7c, 0x40, 0x35
};
static const uint8_t g_keyseed_kirk_c0[0x10] = {
	0x06, 0x9f, 0x49, 0xe1, 0xfb, 0xfe, 0x34, 0x92,
	0xdc, 0x40, 0xfd, 0x2d, 0x1c, 0x86, 0x05, 0x88
};
static const uint8_t g_keyseed_kirk_c1[0x10] = {
	0x53, 0xc3, 0x70, 0x31, 0x81, 0x61, 0x75, 0x43,
	0x3b, 0xb3, 0x43, 0x92, 0xea, 0x9e, 0x20, 0x43
};
static const uint8_t g_keyseed_kirk_c2[0x10] = {
	0xef, 0xf7, 0xd3, 0xb9, 0x55, 0x97, 0xc3, 0x7a,
	0xb4, 0xe5, 0xdc, 0x16, 0x06, 0x65, 0xc4, 0x4c
};
static const uint8_t g_keyseed_kirk_c3[0x10] = {
	0xef, 0xa8, 0xa0, 0x55, 0x93, 0xe0, 0x0d, 0x45,
	0xc3, 0xc1, 0xe1, 0x1d, 0x74, 0xe2, 0x2a, 0x24
};

/* 224-bit ECC curve parameters (g_ec224_p..g_ec224_gy, table at g_ec_curve_224) */
static const uint8_t g_ec224_p[0x1c] = {
	0xa5, 0x3e, 0x11, 0x3e, 0x46, 0xd8, 0xc9, 0xc1,
	0xf0, 0x9d, 0x9b, 0xcb, 0x2a, 0x53, 0x73, 0xd3,
	0x79, 0xf6, 0x9d, 0xa2, 0x8d, 0x09, 0x99, 0x9f,
	0xed, 0x57, 0xa9, 0x0f
};
static const uint8_t g_ec224_a[0x1c] = {
	0xa5, 0x3e, 0x11, 0x3e, 0x46, 0xd8, 0xc9, 0xc1,
	0xf0, 0x9d, 0x9b, 0xcb, 0x2a, 0x53, 0x73, 0xd3,
	0x79, 0xf6, 0x9d, 0xa2, 0x8d, 0x09, 0x99, 0x9f,
	0xed, 0x57, 0xa9, 0x0c
};
static const uint8_t g_ec224_b[0x1c] = {
	0x90, 0x65, 0x94, 0x1d, 0x29, 0x37, 0x4a, 0x8f,
	0x11, 0xdd, 0x1e, 0x54, 0x01, 0x89, 0x43, 0x4e,
	0x4a, 0x6e, 0xbf, 0xaf, 0x54, 0x77, 0xf6, 0xc1,
	0x72, 0xf6, 0x85, 0x5e
};
static const uint8_t g_ec224_n[0x1c] = {
	0xa5, 0x3e, 0x11, 0x3e, 0x46, 0xd8, 0xc9, 0xc1,
	0xf0, 0x9d, 0x9b, 0xcb, 0x2a, 0x52, 0x26, 0x98,
	0xde, 0xef, 0x58, 0xdb, 0x1a, 0xd9, 0xab, 0x7f,
	0x04, 0xe3, 0xae, 0x7f
};
static const uint8_t g_ec224_gx[0x1c] = {
	0x7e, 0x06, 0x09, 0x82, 0x47, 0xe6, 0xb5, 0x9f,
	0x31, 0x10, 0xbc, 0xbb, 0x3a, 0xb6, 0xc2, 0x50,
	0xbc, 0x5a, 0xb0, 0x6c, 0x03, 0x2d, 0xad, 0x43,
	0x68, 0x4c, 0x24, 0x8f
};
static const uint8_t g_ec224_gy[0x1c] = {
	0x0b, 0xd9, 0x41, 0x8d, 0xe8, 0xe3, 0xe4, 0x5d,
	0x2d, 0x70, 0x1e, 0x02, 0x37, 0xfd, 0x7f, 0x2a,
	0xde, 0x0d, 0x48, 0xb7, 0x4c, 0xee, 0xf2, 0xf1,
	0xc8, 0xac, 0x48, 0x4e
};

static const uint8_t * const g_ec_curve_224[6] = {
	g_ec224_p, g_ec224_a, g_ec224_b, g_ec224_n, g_ec224_gx, g_ec224_gy
};

/* 160-bit ECC curve parameters (g_ec160_p..g_ec160_gy, table at g_ec_curve_160) */
static const uint8_t g_ec160_p[0x14] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff
};
static const uint8_t g_ec160_a[0x14] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfc
};
static const uint8_t g_ec160_b[0x14] = {
	0xa6, 0x8b, 0xed, 0xc3, 0x34, 0x18, 0x02, 0x9c,
	0x1d, 0x3c, 0xe3, 0x3b, 0x9a, 0x32, 0x1f, 0xcc,
	0xbb, 0x9e, 0x0f, 0x0b
};
static const uint8_t g_ec160_n[0x14] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	0xff, 0xff, 0xb5, 0xae, 0x3c, 0x52, 0x3e, 0x63,
	0x94, 0x4f, 0x21, 0x27
};
static const uint8_t g_ec160_gx[0x14] = {
	0x12, 0x8e, 0xc4, 0x25, 0x64, 0x87, 0xfd, 0x8f,
	0xdf, 0x64, 0xe2, 0x43, 0x7b, 0xc0, 0xa1, 0xf6,
	0xd5, 0xaf, 0xde, 0x2c
};
static const uint8_t g_ec160_gy[0x14] = {
	0x59, 0x58, 0x55, 0x7e, 0xb1, 0xdb, 0x00, 0x12,
	0x60, 0x42, 0x55, 0x24, 0xdb, 0xc3, 0x79, 0xd5,
	0xac, 0x5f, 0x4a, 0xdf
};

static const uint8_t * const g_ec_curve_160[6] = {
	g_ec160_p, g_ec160_a, g_ec160_b, g_ec160_n, g_ec160_gx, g_ec160_gy
};

static const uint32_t g_bigmac_key_sizes[4] = { 0, 0x10, 0x18, 0x20 };

struct bignum_win_entry {
	uint32_t shift;
	uintptr_t reg_addr;
	uint32_t reg_idx;
};

static const struct bignum_win_entry g_ec_shamir_win_table[4] = {
	{ 1, 0xe0040454u, 0x15u },
	{ 3, 0xe004055cu, 0x57u },
	{ 2, 0xe004055cu, 0x57u },
	{ 3, 0xe00405b0u, 0x6cu }
};

/* .bss globals (0x813304..0x815400) */
static uintptr_t *g_sm_srv_ctx_ptr;
static uint8_t g_bigmac_src_buf[0x800] __attribute__((aligned(64)));
static uint8_t g_bigmac_dst_buf[0x800] __attribute__((aligned(64)));
static uint8_t g_cmac_calc_buf[0x40] __attribute__((aligned(64)));
static uint8_t g_cmac_dec_buf[0x40] __attribute__((aligned(64)));
static uint8_t g_zero_iv_buf[0x40] __attribute__((aligned(64)));
static uint32_t g_mailbox_pkt_buf[0x400] __attribute__((aligned(64)));

/* 0x80b092 */
void sm_stack_chk_fail(void)
{
	sm_syscall_panic(0);
	for (;;)
		;
}

/* 0x80b09c */
int gcauthmgr_main(void)
{
	uintptr_t ctx[0x68];
	uintptr_t *node;

	sm_srv_init(ctx);
	node = sm_srv_alloc_node(ctx);
	if (node == NULL) {
		sm_srv_teardown(ctx);
		return (int)0x800f1816u;
	}
	node[0] = 0x1000bu;
	node[1] = 0;
	if ((uintptr_t)(node + 2) != 0) {
		node[2] = (uintptr_t)g_kirk_service_vtable;
		node[3] = (uintptr_t)kirk_service_dispatch;
	}
	node[1] = ctx[1];
	ctx[1] = (uintptr_t)node;
	sm_srv_run_loop(ctx);
	sm_srv_teardown(ctx);
	return 0;
}

int sm_main(void)
{
	return gcauthmgr_main();
}

/* 0x80b106 */
void sm_service_invoke_cb(void *self, void *req)
{
	void (**fn_ptr)(void *, void *, void *) = (void (**)(void *, void *, void *))self;

	fn_ptr[1](req, req, (void *)(uintptr_t)fn_ptr[1]);
}

/* 0x80b11a */
void keyring_read_slot(uint32_t slot, uint32_t *dst)
{
	uint32_t base = 0xe0058000u + (slot << 5);

	dst[0] = MMIO_READ32(base + 0x00);
	dst[1] = MMIO_READ32(base + 0x04);
	dst[2] = MMIO_READ32(base + 0x08);
	dst[3] = MMIO_READ32(base + 0x0c);
	dst[4] = MMIO_READ32(base + 0x10);
	dst[5] = MMIO_READ32(base + 0x14);
	dst[6] = MMIO_READ32(base + 0x18);
	dst[7] = MMIO_READ32(base + 0x1c);
}

/* 0x80b164 */
int kirk_validate_req_size(const void *req)
{
	const uint8_t *p = (const uint8_t *)req;
	uint32_t cmd = *(const uint32_t *)(p + 0x04);
	uint32_t size = *(const uint32_t *)(p + 0x80c);

	if (cmd == 4 || cmd == 7) {
		if (size > 0x800 || (size & 0xf) != 0 || size == 0)
			return 0x10;
	}
	if (cmd == 0x0c && size != 0)
		return 0x10;
	if (cmd == 0x0d && size != 0x3c)
		return 0x10;
	if (cmd == 0x0e && size != 0)
		return 0x10;
	if ((cmd == 0x10 || cmd == 0x21) && size != 0x34)
		return 0x10;
	if (cmd == 0x11 && size != 0x64)
		return 0x10;
	if (cmd == 0x12 && size != 0xb8)
		return 0x10;
	if (cmd == 0x14 && size != 0)
		return 0x10;
	if (cmd == 0x15 && size != 0x54)
		return 0x10;
	if (cmd == 0x16 && size != 0)
		return 0x10;
	if ((cmd == 0x17 || cmd == 0x22) && size != 0x3c)
		return 0x10;
	if (cmd == 0x18 && size != 0x8c)
		return 0x10;
	if (cmd == 0x19 && size != 0xe8)
		return 0x10;
	if (cmd == 0x1b && size != 0x53)
		return 0x10;
	if (cmd == 0x1c && size != 0x40)
		return 0x10;
	if (cmd == 0x1d && size != 0xa3)
		return 0x10;
	if (cmd == 0x1e && size != 0x51)
		return 0x10;
	if (cmd == 0x1f && size != 0xb3)
		return 0x10;
	if (cmd == 0x20 && size != 0x116)
		return 0x10;
	if (cmd == 0x23 && size != 0x20)
		return 0x10;
	if (cmd == 0x24 && size != 0x80)
		return 0x10;
	if (cmd == 0x25 && size != 0x50)
		return 0x10;
	return 0;
}

/* 0x80b2ee */
int kirk_cmd_0d_ec_mul_160(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t scalar[20], in_x[20], in_y[20], out_x[20], out_y[20];
	uint8_t *in_xy[2] = { in_x, in_y };
	uint8_t *out_xy[2] = { out_x, out_y };

	sm_memcpy(in_x, p + 0x1c, 0x14);
	sm_memcpy(in_y, p + 0x30, 0x14);
	sm_memcpy(scalar, p + 0x08, 0x14);
	if (ec_point_mul_160(out_xy, scalar, in_xy, g_ec_curve_160) == 0) {
		sm_memcpy(p + 0x08, out_xy[0], 0x14);
		sm_memcpy(p + 0x1c, out_xy[1], 0x14);
		*(uint32_t *)(p + 0x80c) = 0x28;
		return 0;
	}
	return 5;
}

/* 0x80b398 */
int kirk_cmd_11_ecdsa_verify_160(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t r_sig[20], s_sig[20], hash[20], pub_x[20], pub_y[20];
	uint8_t *sig_rs[2] = { r_sig, s_sig };
	uint8_t *pub_xy[2] = { pub_x, pub_y };

	sm_memcpy(r_sig, p + 0x08, 0x14);
	sm_memcpy(s_sig, p + 0x1c, 0x14);
	sm_memcpy(hash, p + 0x30, 0x14);
	sm_memcpy(pub_x, p + 0x44, 0x14);
	sm_memcpy(pub_y, p + 0x58, 0x14);
	if (ecdsa_verify_160(pub_xy, hash, sig_rs, g_ec_curve_160) == 0)
		return 0;
	return 5;
}

/* 0x80b432 */
uint32_t bigmac_derive_keyslot_ecb(const void *key_src, uint32_t dst_slot, uint32_t src_slot)
{
	uint32_t status;

	MMIO_WRITE32(0xe0030028u, dst_slot & 0xffffu);
	(void)MMIO_READ32(0xe003002cu);
	MMIO_WRITE32(0xe0050000u, MMIO_ADDPTR(key_src));
	MMIO_WRITE32(0xe0050004u, dst_slot);
	MMIO_WRITE32(0xe0050008u, 0x10u);
	MMIO_WRITE32(0xe005000cu, 0x10000302u);
	MMIO_WRITE32(0xe0050010u, src_slot & 0x7ffu);
	MMIO_WRITE32(0xe005001cu, 1u);
	do {
		status = MMIO_READ32(0xe0050024u);
	} while ((status & 1u) != 0);
	MMIO_WRITE32(0xe0030028u, dst_slot & 0xffffu);
	if ((MMIO_READ32(0xe003002cu) & 2u) == 0)
		status = 0x11u;
	return status;
}

/* 0x80b4ac */
uint32_t bigmac_derive_keyslot_cbc(const void *key_src, const void *iv_src, uint32_t dst_slot, uint32_t src_slot)
{
	uint32_t status;

	MMIO_WRITE32(0xe0030028u, dst_slot & 0xffffu);
	(void)MMIO_READ32(0xe003002cu);
	MMIO_WRITE32(0xe0050000u, MMIO_ADDPTR(key_src));
	MMIO_WRITE32(0xe0050004u, dst_slot);
	MMIO_WRITE32(0xe0050008u, 0x10u);
	MMIO_WRITE32(0xe005000cu, 0x1000010au);
	sm_memcpy(g_zero_iv_buf, iv_src, 0x10);
	MMIO_WRITE32(0xe0050014u, MMIO_ADDPTR(g_zero_iv_buf));
	MMIO_WRITE32(0xe0050010u, src_slot & 0x7ffu);
	MMIO_WRITE32(0xe005001cu, 1u);
	do {
		status = MMIO_READ32(0xe0050024u);
	} while ((status & 1u) != 0);
	MMIO_WRITE32(0xe0030028u, dst_slot & 0xffffu);
	if ((MMIO_READ32(0xe003002cu) & 2u) == 0)
		status = 0x11u;
	return status;
}

/* 0x80b55e */
uint32_t bigmac_aes_cbc_keyslot(void *dst, const void *src, uint32_t len, uint32_t keyslot, uint32_t mode)
{
	if (src == NULL || dst == NULL)
		return 0x12u;
	MMIO_WRITE32(0xe0030028u, keyslot & 0xffffu);
	if ((MMIO_READ32(0xe003002cu) & 2u) != 2u)
		return 0x11u;
	MMIO_WRITE32(0xe0050008u, len & 0xffffffu);
	MMIO_WRITE32(0xe005000cu, (mode & 7u) | 0x108u);
	MMIO_WRITE32(0xe0050000u, MMIO_ADDPTR(src));
	MMIO_WRITE32(0xe0050004u, MMIO_ADDPTR(dst));
	sm_memset(g_zero_iv_buf, 0, 0x10);
	MMIO_WRITE32(0xe0050014u, MMIO_ADDPTR(g_zero_iv_buf));
	MMIO_WRITE32(0xe0050010u, keyslot & 0x7ffu);
	MMIO_WRITE32(0xe005001cu, 1u);
	while ((MMIO_READ32(0xe0050024u) & 1u) != 0)
		;
	return MMIO_READ32(0xe0050024u);
}

/* 0x80b614 */
uint32_t bigmac_load_keyslot_204(const void *key_src)
{
	uint32_t status;

	MMIO_WRITE32(0xe0030028u, 0u);
	(void)MMIO_READ32(0xe003002cu);
	MMIO_WRITE32(0xe0050000u, MMIO_ADDPTR(key_src));
	MMIO_WRITE32(0xe0050004u, 0u);
	MMIO_WRITE32(0xe0050008u, 0x10u);
	MMIO_WRITE32(0xe005000cu, 0x10000101u);
	MMIO_WRITE32(0xe0050010u, 0x204u);
	MMIO_WRITE32(0xe005001cu, 1u);
	do {
		status = MMIO_READ32(0xe0050024u);
	} while ((status & 1u) != 0);
	MMIO_WRITE32(0xe0030028u, status & 1u);
	if ((MMIO_READ32(0xe003002cu) & 2u) == 0)
		status = 0x11u;
	return status;
}

/* 0x80b6a0 */
uint32_t bigmac_aes_ecb_slot0(void *dst, const void *src, uint32_t len)
{
	if (src == NULL || dst == NULL || len > 0x20u)
		return 0x12u;
	MMIO_WRITE32(0xe0030028u, 0u);
	if ((MMIO_READ32(0xe003002cu) & 2u) != 2u)
		return 0x11u;
	MMIO_WRITE32(0xe0050008u, len & 0xffffffu);
	MMIO_WRITE32(0xe005000cu, 0x10au);
	MMIO_WRITE32(0xe0050000u, MMIO_ADDPTR(src));
	MMIO_WRITE32(0xe0050004u, MMIO_ADDPTR(dst));
	sm_memset(g_zero_iv_buf, 0, 0x10);
	MMIO_WRITE32(0xe0050014u, MMIO_ADDPTR(g_zero_iv_buf));
	MMIO_WRITE32(0xe0050010u, 0u);
	MMIO_WRITE32(0xe005001cu, 1u);
	while ((MMIO_READ32(0xe0050024u) & 1u) != 0)
		;
	return MMIO_READ32(0xe0050024u);
}

/* 0x80b750 */
int kirk_cmd_15_ec_mul_224(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t scalar[28], in_x[28], in_y[28], out_x[28], out_y[28];
	uint8_t *in_xy[2] = { in_x, in_y };
	uint8_t *out_xy[2] = { out_x, out_y };

	sm_memcpy(in_x, p + 0x24, 0x1c);
	sm_memcpy(in_y, p + 0x40, 0x1c);
	sm_memcpy(scalar, p + 0x08, 0x1c);
	if (ec_point_mul_224(out_xy, scalar, in_xy, g_ec_curve_224) == 0) {
		sm_memcpy(p + 0x08, out_xy[0], 0x1c);
		sm_memcpy(p + 0x24, out_xy[1], 0x1c);
		*(uint32_t *)(p + 0x80c) = 0x38;
		return 0;
	}
	return 5;
}

/* 0x80b800 */
int kirk_cmd_18_ecdsa_verify_224(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t r_sig[28], s_sig[28], hash[28], pub_x[28], pub_y[28];
	uint8_t *sig_rs[2] = { r_sig, s_sig };
	uint8_t *pub_xy[2] = { pub_x, pub_y };

	sm_memcpy(r_sig, p + 0x08, 0x1c);
	sm_memcpy(s_sig, p + 0x24, 0x1c);
	sm_memcpy(hash, p + 0x40, 0x1c);
	sm_memcpy(pub_x, p + 0x5c, 0x1c);
	sm_memcpy(pub_y, p + 0x78, 0x1c);
	if (ecdsa_verify_224(pub_xy, hash, sig_rs, g_ec_curve_224) == 0)
		return 0;
	return 5;
}

/* 0x80b8a2 */
uint32_t bigmac_cmac_keyslot(void *dst, const void *src, uint32_t len, uint32_t keyslot)
{
	if (src == NULL || dst == NULL)
		return 0x12u;
	MMIO_WRITE32(0xe0030028u, keyslot & 0xffffu);
	if (((MMIO_READ32(0xe003002cu) & 2u) >> 1) != 1u)
		return 0x11u;
	MMIO_WRITE32(0xe0050000u, MMIO_ADDPTR(src));
	MMIO_WRITE32(0xe0050004u, MMIO_ADDPTR(dst));
	MMIO_WRITE32(0xe0050008u, len & 0xffffffu);
	MMIO_WRITE32(0xe005000cu, 0x13bu);
	MMIO_WRITE32(0xe0050010u, keyslot & 0x7ffu);
	MMIO_WRITE32(0xe005001cu, 1u);
	while ((MMIO_READ32(0xe0050024u) & 1u) != 0)
		;
	return MMIO_READ32(0xe0050024u);
}

/* 0x80b914 */
int bigmac_wait_done(uint32_t *ctx)
{
	uint32_t status;

	do {
		bigmac_ctx_get_status(ctx, &status);
	} while ((status & 1u) != 0);
	return (status == 0) ? 0 : 0x11;
}

/* 0x80b944 */
int kirk_aes_cbc_encrypt(void *buf, uint32_t len, const void *key, int mode)
{
	uint32_t ctx[2];
	uint8_t iv[16];
	int err;

	sm_memcpy(g_bigmac_src_buf, buf, len);
	if (mode == 2) {
		err = (int)bigmac_aes_cbc_keyslot(g_bigmac_dst_buf, g_bigmac_src_buf, len, 0x24u, 1u);
		if (err != 0)
			return err;
	} else {
		bigmac_ctx_init_ch0(ctx);
		sm_memset(iv, 0, 0x10);
		if (bigmac_aes128_cbc_encrypt(g_bigmac_dst_buf, g_bigmac_src_buf, len, key, iv, ctx) != 0)
			return 0x11;
		bigmac_wait_done(ctx);
	}
	sm_memcpy(buf, g_bigmac_dst_buf, len);
	return 0;
}

/* 0x80b9e4 */
int kirk_aes_cbc_decrypt(void *buf, uint32_t len, const void *key, int mode)
{
	uint32_t ctx[2];
	uint8_t iv[16];
	int err;

	sm_memcpy(g_bigmac_src_buf, buf, len);
	if (mode == 2) {
		err = (int)bigmac_aes_cbc_keyslot(g_bigmac_dst_buf, g_bigmac_src_buf, len, 0x24u, 2u);
		if (err != 0)
			return err;
	} else {
		bigmac_ctx_init_ch0(ctx);
		sm_memset(iv, 0, 0x10);
		if (bigmac_aes128_cbc_decrypt(g_bigmac_dst_buf, g_bigmac_src_buf, len, key, iv, ctx) != 0)
			return 0x11;
		bigmac_wait_done(ctx);
	}
	sm_memcpy(buf, g_bigmac_dst_buf, len);
	return 0;
}

/* 0x80ba82 */
int kirk_aes_cmac(const void *src, uint32_t len, const void *key, void *dst_mac)
{
	uint32_t ctx[2];

	sm_memcpy(g_bigmac_src_buf, src, len);
	bigmac_ctx_init_ch0(ctx);
	if (bigmac_aes128_cmac_inline(g_bigmac_dst_buf, g_bigmac_src_buf, len, key, 0x10u, ctx) == 0) {
		bigmac_wait_done(ctx);
		sm_memcpy(dst_mac, g_bigmac_dst_buf, 0x10);
		return 0;
	}
	return 0x11;
}

/* 0x80bae8 */
int kirk_sha256(const void *src, uint32_t len, void *dst_hash)
{
	uint32_t ctx[2];

	sm_memcpy(g_bigmac_src_buf, src, len);
	bigmac_ctx_init_ch0(ctx);
	if (bigmac_sha256_start(g_bigmac_dst_buf, g_bigmac_src_buf, len, ctx) == 0) {
		bigmac_wait_done(ctx);
		sm_memcpy(dst_hash, g_bigmac_dst_buf, 0x20);
		return 0;
	}
	return 0x11;
}

/* 0x80bb3e */
int kirk_sha1(const void *src, uint32_t len, void *dst_hash)
{
	uint32_t ctx[2];

	sm_memcpy(g_bigmac_src_buf, src, len);
	bigmac_ctx_init_ch0(ctx);
	if (bigmac_sha1_start(g_bigmac_dst_buf, g_bigmac_src_buf, len, ctx) == 0) {
		bigmac_wait_done(ctx);
		sm_memcpy(dst_hash, g_bigmac_dst_buf, 0x14);
		return 0;
	}
	return 0x11;
}

/* 0x80bb94 */
int kirk_derive_session_key(const void *seed, uint32_t key_id, void *out_key, uint32_t *out_mode)
{
	const uint8_t *key_seed;
	const uint8_t *iv_seed;
	int err;

	if (key_id == 0x8001u) {
		iv_seed = NULL;
		key_seed = g_keyseed_session_8001;
	} else if (key_id < 0x8002u) {
		if (key_id != 1u)
			return 0x0e;
		iv_seed = g_ivseed_session_0001;
		key_seed = g_keyseed_session_0001;
	} else if (key_id == 0x8002u) {
		iv_seed = NULL;
		key_seed = g_keyseed_session_8002;
	} else if (key_id == 0x8003u) {
		iv_seed = NULL;
		key_seed = g_keyseed_session_8003;
	} else {
		return 0x0e;
	}

	err = (int)bigmac_derive_keyslot_ecb(key_seed, 0x21u, 0x345u);
	if (err == 0) {
		sm_memcpy(g_bigmac_src_buf, seed, 0x20);
		err = (int)bigmac_cmac_keyslot(g_bigmac_dst_buf, g_bigmac_src_buf, 0x20u, 0x21u);
		if (err == 0) {
			sm_memcpy(out_key, g_bigmac_dst_buf, 0x10);
			if (key_id - 1u < 3u) {
				sm_memcpy(g_bigmac_src_buf, out_key, 0x10);
				err = (int)bigmac_derive_keyslot_cbc(g_bigmac_src_buf, iv_seed, 0x24u, 0x348u);
				if (err == 0)
					*out_mode = 2u;
			} else {
				*out_mode = 1u;
			}
		}
	}
	return err;
}

/* 0x80bc6a */
int kirk_cmd_1b_verify_challenge(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint32_t mode = 0;
	uint8_t buf[32];
	uint8_t key[16];
	int err;

	err = kirk_derive_session_key(p + 0x08, *(uint32_t *)(p + 0x808), key, &mode);
	if (err == 0) {
		sm_memcpy(buf, p + 0x3b, 0x20);
		kirk_aes_cbc_decrypt(buf, 0x20, key, (int)mode);
		if (sm_memcmp_consttime(p + 0x29, buf + 0x11, 0x0f) != 0)
			err = 5;
	}
	return err;
}

/* 0x80bcdc */
int kirk_decrypt_session_key(const void *seed, uint32_t key_id, const void *enc_in, void *dec_out)
{
	uint32_t mode = 0;
	uint8_t key[16];
	int err;

	err = kirk_derive_session_key(seed, key_id, key, &mode);
	if (err == 0) {
		sm_memcpy(dec_out, enc_in, 0x10);
		err = kirk_aes_cbc_decrypt(dec_out, 0x10, key, (int)mode);
	}
	return err;
}

/* 0x80bd2c */
int kirk_cmd_20_verify_key_blocks(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t work[0x50];
	uint8_t kpart1[0x20];
	uint8_t kpart2[0x20];
	uint8_t blk58[0x10];
	uint8_t mac[0x10];
	uint8_t sess_key[0x10];
	int err;

	err = kirk_decrypt_session_key(p + 0x08, *(uint32_t *)(p + 0x808), p + 0x28, sess_key);
	if (err != 0)
		return err;
	sm_memcpy(work, p + 0x78, 3);
	sm_memset(work + 3, 0, 0x0d);
	sm_memcpy(work + 0x10, p + 0x7b, 0x30);
	kirk_aes_cmac(work, 0x40, sess_key, mac);
	if (sm_memcmp_consttime(p + 0xab, mac, 0x10) == 0) {
		sm_memcpy(work, p + 0x58, 0x20);
		kirk_aes_cbc_decrypt(work, 0x20, sess_key, 1);
		sm_memcpy(blk58, work, 0x10);
		sm_memcpy(work, p + 0x7b, 0x30);
		kirk_aes_cbc_decrypt(work, 0x30, sess_key, 1);
		if (sm_memcmp_consttime(blk58 + 1, work + 1, 0x0f) == 0) {
			if (work[0x1f] != 3)
				return 0x12;
			sm_memcpy(kpart1, work + 0x10, 0x20);
			sm_memcpy(work, p + 0xcb, 3);
			sm_memset(work + 3, 0, 0x0d);
			sm_memcpy(work + 0x10, p + 0xce, 0x40);
			kirk_aes_cmac(work, 0x50, sess_key, mac);
			if (sm_memcmp_consttime(p + 0x10e, mac, 0x10) == 0) {
				sm_memcpy(work, p + 0xce, 0x40);
				kirk_aes_cbc_decrypt(work, 0x40, sess_key, 1);
				if (sm_memcmp_consttime(p + 0xbc, work + 9, 0x0f) == 0) {
					sm_memcpy(kpart2, work + 0x18, 0x20);
					*(uint32_t *)(p + 0x80c) = 0x34;
					sm_memcpy(work, kpart2, 0x20);
					sm_memcpy(work + 0x20, kpart1, 0x20);
					kirk_sha256(work, 0x40, p + 0x08);
					kirk_sha1(kpart2, 0x20, p + 0x28);
					return err;
				}
			}
		}
	}
	return 5;
}

/* 0x80beea */
int kirk_cmd_1f_decrypt_session_block(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t work[0x40];
	uint8_t blk58[0x10];
	uint8_t mac[0x10];
	uint8_t sess_key[0x10];
	int err;

	err = kirk_decrypt_session_key(p + 0x08, *(uint32_t *)(p + 0x808), p + 0x28, sess_key);
	if (err == 0) {
		sm_memcpy(work, p + 0x78, 3);
		sm_memset(work + 3, 0, 0x0d);
		sm_memcpy(work + 0x10, p + 0x7b, 0x30);
		kirk_aes_cmac(work, 0x40, sess_key, mac);
		if (sm_memcmp_consttime(p + 0xab, mac, 0x10) == 0) {
			sm_memcpy(work, p + 0x58, 0x20);
			kirk_aes_cbc_decrypt(work, 0x20, sess_key, 1);
			sm_memcpy(blk58, work, 0x10);
			sm_memcpy(work, p + 0x7b, 0x30);
			kirk_aes_cbc_decrypt(work, 0x30, sess_key, 1);
			if (sm_memcmp_consttime(blk58 + 1, work + 1, 0x0f) == 0) {
				if (work[0x1f] == 3) {
					err = 0x12;
				} else {
					*(uint32_t *)(p + 0x80c) = 0x20;
					sm_memcpy(p + 0x08, work + 0x10, 0x20);
				}
				return err;
			}
		}
		err = 5;
	}
	return err;
}

/* 0x80bfe6 */
int kirk_cmd_1d_verify_cert_block(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t work[0x40];
	uint8_t sess_key[0x10];
	int err;

	err = kirk_decrypt_session_key(p + 0x08, *(uint32_t *)(p + 0x808), p + 0x28, sess_key);
	if (err == 0) {
		sm_memcpy(work, p + 0x6b, 0x40);
		kirk_aes_cbc_decrypt(work, 0x40, sess_key, 1);
		if (sm_memcmp_consttime(p + 0x59, work + 9, 0x0f) != 0 ||
		    sm_memcmp_consttime(p + 0x08, work + 0x18, 0x20) != 0)
			err = 5;
	}
	return err;
}

/* 0x80c070 */
int kirk_cmd_23_encrypt_and_cmac(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint32_t ctx[2];
	uint8_t iv[16];

	sm_memcpy(g_bigmac_src_buf, p + 0x08, 0x10);
	bigmac_ctx_init_ch0(ctx);
	sm_memset(iv, 0, 0x10);
	if (bigmac_aes128_cbc_encrypt(g_bigmac_dst_buf, g_bigmac_src_buf, 0x10, g_key_kirk23_enc, iv, ctx) == 0) {
		bigmac_wait_done(ctx);
		bigmac_ctx_init_ch0(ctx);
		if (bigmac_aes128_cmac_inline(g_bigmac_dst_buf + 0x10, g_bigmac_dst_buf, 0x10, g_key_kirk23_cmac, 0x10, ctx) == 0) {
			bigmac_wait_done(ctx);
			*(uint32_t *)(p + 0x80c) = 0x20;
			sm_memcpy(p + 0x08, g_bigmac_dst_buf, 0x20);
			return 0;
		}
	}
	return 0x11;
}

/* 0x80c11c */
int kirk_hmac_sha256_init_state(const void *part_a, const void *part_b, uint32_t part_len, void *out_digest)
{
	uint32_t ctx[2];

	if (part_len != 0x14u && part_len != 0x1cu)
		return 0x12;
	sm_memcpy(g_bigmac_src_buf, part_b, part_len);
	sm_memcpy(g_bigmac_src_buf + part_len, part_a, part_len);
	bigmac_ctx_init_ch0(ctx);
	if (bigmac_hmac_sha256_start(g_bigmac_dst_buf, g_bigmac_src_buf, part_len << 1, g_drbg_hmac_init_key, 0x40u, ctx) == 0) {
		bigmac_wait_done(ctx);
		sm_memcpy(out_digest, g_bigmac_dst_buf, 0x20);
		return 0;
	}
	return 0x11;
}

/* 0x80c1a0 */
int kirk_hmac_sha256_drbg_step(const uint8_t *seed, uint32_t seed_len, uint8_t *state, uint8_t *out_block)
{
	uint32_t ctx[2];
	uint8_t tmp[32];
	uint32_t i;

	if (seed_len > 0x20u)
		return 0x12;
	sm_memcpy(tmp, state, 0x20);
	for (i = 0; i < seed_len; i++)
		tmp[i] ^= seed[i];
	sm_memcpy(g_bigmac_src_buf, tmp, 0x20);
	bigmac_ctx_init_ch0(ctx);
	if (bigmac_sha256_start(g_bigmac_dst_buf, g_bigmac_src_buf, 0x20, ctx) != 0)
		return 0x11;
	bigmac_wait_done(ctx);
	sm_memcpy(out_block, g_bigmac_dst_buf, 0x20);
	for (i = 0; i < 0x20; i++)
		state[i] ^= out_block[i];
	for (i = 0; i < 0x20; i++) {
		if (state[i] != 0xffu) {
			state[i]++;
			break;
		}
		state[i] = 0;
	}
	return 0;
}

/* 0x80c27c */
int kirk_cmd_22_ecdsa_sign_224_det(void *req)
{
	uint8_t *p = (uint8_t *)req;
	const uint8_t *priv_key;
	uint8_t rnd64[64];
	uint8_t state[32];
	uint8_t hash[28], r_sig[28], s_sig[28], nonce[28];
	uint8_t *sig_rs[2] = { r_sig, s_sig };
	uint32_t i;

	if (*(uint32_t *)(p + 0x808) == 0)
		priv_key = g_ec224_privkey_0;
	else if (*(uint32_t *)(p + 0x808) == 1)
		priv_key = g_ec224_privkey_1;
	else
		return 0x12;

	sm_memcpy(hash, p + 0x28, 0x1c);
	if (kirk_hmac_sha256_init_state(priv_key, hash, 0x1c, state) == 0) {
		do {
			if (kirk_hmac_sha256_drbg_step(hash, 0x1c, state, rnd64) != 0 ||
			    kirk_hmac_sha256_drbg_step(hash, 0x1c, state, rnd64 + 0x20) != 0)
				return 5;
		} while (bignum_mod_reduce_224(nonce, rnd64, g_ec_curve_224[3]) != 0);
		for (i = 0; i < 0x20; i++) {
			if (p[0x08 + i] != 0)
				return 5;
		}
		sm_memcpy(g_bigmac_dst_buf, priv_key, 0x1c);
		if (ecdsa_sign_224(sig_rs, hash, nonce, g_bigmac_dst_buf, g_ec_curve_224) == 0) {
			sm_memcpy(p + 0x08, sig_rs[0], 0x1c);
			sm_memcpy(p + 0x24, sig_rs[1], 0x1c);
			*(uint32_t *)(p + 0x80c) = 0x38;
			return 0;
		}
	}
	return 5;
}

/* 0x80c390 */
int kirk_cmd_21_ecdsa_sign_160_det(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t rnd64[64];
	uint8_t state[32];
	uint8_t hash[20], r_sig[20], s_sig[20], nonce[20];
	uint8_t *sig_rs[2] = { r_sig, s_sig };
	uint32_t i;

	sm_memcpy(hash, p + 0x28, 0x14);
	if (kirk_hmac_sha256_init_state(g_ec160_privkey, hash, 0x14, state) == 0) {
		do {
			if (kirk_hmac_sha256_drbg_step(hash, 0x14, state, rnd64) != 0 ||
			    kirk_hmac_sha256_drbg_step(hash, 0x14, state, rnd64 + 0x20) != 0)
				return 5;
		} while (bignum_mod_reduce_160(nonce, rnd64, g_ec_curve_160[3]) != 0);
		for (i = 0; i < 0x20; i++) {
			if (p[0x08 + i] != 0)
				return 5;
		}
		sm_memcpy(g_bigmac_dst_buf, g_ec160_privkey, 0x14);
		if (ecdsa_sign_160(sig_rs, hash, nonce, g_bigmac_dst_buf, g_ec_curve_160) == 0) {
			sm_memcpy(p + 0x08, sig_rs[0], 0x14);
			sm_memcpy(p + 0x1c, sig_rs[1], 0x14);
			*(uint32_t *)(p + 0x80c) = 0x28;
			return 0;
		}
	}
	return 5;
}

/* 0x80c488 */
int kirk_rng_hmac_sha256(void *dst, uint32_t len)
{
	uint32_t ctx[2];
	uint32_t *rng_buf = (uint32_t *)(uintptr_t)g_bigmac_src_buf;
	uint32_t off, i;

	if ((len & 0x1fu) != 0)
		return 0x12;
	for (off = 0; off < len; off += 0x20u) {
		for (i = 0; i < 8; i++)
			rng_buf[i] = MMIO_READ32(0xe005003cu);
		bigmac_ctx_init_ch0(ctx);
		if (bigmac_hmac_sha256_start(g_bigmac_dst_buf, g_bigmac_src_buf, 0x20u, g_rng_hmac_key, 0x40u, ctx) != 0)
			return 0x11;
		bigmac_wait_done(ctx);
		sm_memcpy((uint8_t *)dst + off, g_bigmac_dst_buf, 0x20);
	}
	return 0;
}

/* 0x80c51c */
int kirk_cmd_1e_gen_challenge_b1(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t mac_in[0x30];
	uint8_t plain[0x20];
	uint8_t rnd[0x20];
	uint8_t mac[0x10];
	uint8_t sess_key[0x10];
	int err;

	err = kirk_decrypt_session_key(p + 0x08, *(uint32_t *)(p + 0x808), p + 0x28, sess_key);
	if (err == 0) {
		if (kirk_rng_hmac_sha256(rnd, 0x20) == 0) {
			sm_memcpy(plain, rnd, 0x10);
			plain[0] |= 0x80u;
			sm_memset(plain + 0x10, 0, 0x10);
			plain[0x1f] = p[0x58];
			kirk_aes_cbc_encrypt(plain, 0x20, sess_key, 1);
			mac_in[0] = 0xb1u;
			mac_in[1] = 0x00u;
			mac_in[2] = 0x33u;
			sm_memset(mac_in + 3, 0, 0x0d);
			sm_memcpy(mac_in + 0x10, plain, 0x20);
			kirk_aes_cmac(mac_in, 0x30, sess_key, mac);
			*(uint32_t *)(p + 0x80c) = 0x33;
			p[0x08] = 0xb1u;
			p[0x09] = 0x00u;
			p[0x0a] = 0x33u;
			sm_memcpy(p + 0x0b, plain, 0x20);
			sm_memcpy(p + 0x2b, mac, 0x10);
			return 0;
		}
		err = 5;
	}
	return err;
}

/* 0x80c62a */
int kirk_cmd_1c_gen_token_a3(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint32_t mode = 0;
	uint8_t buf[0x30];
	uint8_t rnd[0x20];
	uint8_t b0[0x10], b1[0x10], sess_key[0x10];
	int err;

	err = kirk_derive_session_key(p + 0x08, *(uint32_t *)(p + 0x808), sess_key, &mode);
	if (err == 0) {
		sm_memcpy(buf, p + 0x28, 0x20);
		kirk_aes_cbc_decrypt(buf, 0x20, sess_key, (int)mode);
		sm_memcpy(b0, buf, 0x10);
		sm_memcpy(b1, buf + 0x10, 0x10);
		b0[0] |= 0x80u;
		b1[0] |= 0x80u;
		if (kirk_rng_hmac_sha256(rnd, 0x20) == 0) {
			sm_memcpy(buf, rnd, 0x10);
			sm_memcpy(buf + 0x10, b1, 0x10);
			sm_memcpy(buf + 0x20, b0, 0x10);
			kirk_aes_cbc_encrypt(buf, 0x30, sess_key, (int)mode);
			*(uint32_t *)(p + 0x80c) = 0x33;
			p[0x08] = 0xa3u;
			p[0x09] = 0x00u;
			p[0x0a] = 0x33u;
			sm_memcpy(p + 0x0b, buf, 0x30);
			return 0;
		}
		err = 5;
	}
	return err;
}

/* 0x80c722 */
int kirk_cmd_17_ecdsa_sign_224(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t rnd64[64];
	uint8_t hash[28], r_sig[28], s_sig[28], nonce[28];
	uint8_t *sig_rs[2] = { r_sig, s_sig };
	int err;

	do {
		if (kirk_rng_hmac_sha256(rnd64, 0x40) != 0)
			return 5;
	} while (bignum_mod_reduce_224(nonce, rnd64, g_ec_curve_224[3]) != 0);

	err = (int)bigmac_load_keyslot_204(g_keyseed_kirk_10_17);
	if (err == 0) {
		sm_memcpy(g_bigmac_src_buf, p + 0x08, 0x20);
		err = (int)bigmac_aes_ecb_slot0(g_bigmac_dst_buf, g_bigmac_src_buf, 0x20);
		if (err == 0) {
			sm_memcpy(hash, p + 0x28, 0x1c);
			if (ecdsa_sign_224(sig_rs, hash, nonce, g_bigmac_dst_buf, g_ec_curve_224) == 0) {
				sm_memcpy(p + 0x08, sig_rs[0], 0x1c);
				sm_memcpy(p + 0x24, sig_rs[1], 0x1c);
				*(uint32_t *)(p + 0x80c) = 0x38;
			} else {
				err = 5;
			}
		}
	}
	return err;
}

/* 0x80c800 */
int kirk_cmd_16_rng_28(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t rnd[32];

	if (kirk_rng_hmac_sha256(rnd, 0x20) == 0) {
		sm_memcpy(p + 0x08, rnd, 0x1c);
		*(uint32_t *)(p + 0x80c) = 0x1c;
		return 0;
	}
	return 5;
}

/* 0x80c84e */
int kirk_cmd_14_ecdsa_keygen_224(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t rnd64[64];
	uint8_t priv[28], pub_x[28], pub_y[28];
	uint8_t *pub_xy[2] = { pub_x, pub_y };

	do {
		if (kirk_rng_hmac_sha256(rnd64, 0x40) != 0)
			return 5;
	} while (bignum_mod_reduce_224(priv, rnd64, g_ec_curve_224[3]) != 0);

	if (ec_pubkey_from_priv_224(pub_xy, priv, g_ec_curve_224) == 0) {
		sm_memcpy(p + 0x08, priv, 0x1c);
		sm_memcpy(p + 0x24, pub_xy[0], 0x1c);
		sm_memcpy(p + 0x40, pub_xy[1], 0x1c);
		*(uint32_t *)(p + 0x80c) = 0x54;
		return 0;
	}
	return 5;
}

/* 0x80c8f0 */
int kirk_cmd_10_ecdsa_sign_160(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t rnd64[64];
	uint8_t hash[20], r_sig[20], s_sig[20], nonce[20];
	uint8_t *sig_rs[2] = { r_sig, s_sig };
	int err;

	do {
		if (kirk_rng_hmac_sha256(rnd64, 0x40) != 0)
			return 5;
	} while (bignum_mod_reduce_160(nonce, rnd64, g_ec_curve_160[3]) != 0);

	err = (int)bigmac_load_keyslot_204(g_keyseed_kirk_10_17);
	if (err == 0) {
		sm_memcpy(g_bigmac_src_buf, p + 0x08, 0x20);
		err = (int)bigmac_aes_ecb_slot0(g_bigmac_dst_buf, g_bigmac_src_buf, 0x20);
		if (err == 0) {
			sm_memcpy(hash, p + 0x28, 0x14);
			if (ecdsa_sign_160(sig_rs, hash, nonce, g_bigmac_dst_buf, g_ec_curve_160) == 0) {
				sm_memcpy(p + 0x08, sig_rs[0], 0x14);
				sm_memcpy(p + 0x1c, sig_rs[1], 0x14);
				*(uint32_t *)(p + 0x80c) = 0x28;
			} else {
				err = 5;
			}
		}
	}
	return err;
}

/* 0x80c9cc */
int kirk_cmd_0e_rng_20(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t rnd[32];

	if (kirk_rng_hmac_sha256(rnd, 0x20) == 0) {
		sm_memcpy(p + 0x08, rnd, 0x14);
		*(uint32_t *)(p + 0x80c) = 0x14;
		return 0;
	}
	return 5;
}

/* 0x80ca1a */
int kirk_cmd_0c_ecdsa_keygen_160(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t rnd64[64];
	uint8_t priv[20], pub_x[20], pub_y[20];
	uint8_t *pub_xy[2] = { pub_x, pub_y };

	do {
		if (kirk_rng_hmac_sha256(rnd64, 0x40) != 0)
			return 5;
	} while (bignum_mod_reduce_160(priv, rnd64, g_ec_curve_160[3]) != 0);

	if (ec_pubkey_from_priv_160(pub_xy, priv, g_ec_curve_160) == 0) {
		sm_memcpy(p + 0x08, priv, 0x14);
		sm_memcpy(p + 0x1c, pub_xy[0], 0x14);
		sm_memcpy(p + 0x30, pub_xy[1], 0x14);
		*(uint32_t *)(p + 0x80c) = 0x3c;
		return 0;
	}
	return 5;
}

/* 0x80caba */
int kirk_derive_key_509_50d(void *out_key)
{
	uint32_t slot[8];
	uint32_t ctx[2];
	uint8_t iv[16];
	uint8_t key_602[16];
	uint8_t blk_601[16];
	int i;

	keyring_read_slot(0x602u, slot);
	for (i = 0; i < 4; i++) {
		uint32_t w = slot[7 - i];
		key_602[i * 4 + 0] = (uint8_t)(w >> 24);
		key_602[i * 4 + 1] = (uint8_t)(w >> 16);
		key_602[i * 4 + 2] = (uint8_t)(w >> 8);
		key_602[i * 4 + 3] = (uint8_t)w;
	}
	keyring_read_slot(0x601u, slot);
	for (i = 0; i < 4; i++) {
		uint32_t w = slot[3 - i];
		blk_601[i * 4 + 0] = (uint8_t)(w >> 24);
		blk_601[i * 4 + 1] = (uint8_t)(w >> 16);
		blk_601[i * 4 + 2] = (uint8_t)(w >> 8);
		blk_601[i * 4 + 3] = (uint8_t)w;
	}
	bigmac_ctx_init_ch0(ctx);
	sm_memset(iv, 0, 0x10);
	if (bigmac_aes128_cbc_encrypt(out_key, blk_601, 0x10, key_602, iv, ctx) == 0) {
		bigmac_wait_done(ctx);
		return 0;
	}
	return 2;
}

/* 0x80cc28 */
int kirk_cmd_07_decrypt_key100(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint32_t ctx[2];
	uint8_t iv[16];
	uint8_t key[16];
	uint32_t len;
	int err;

	err = kirk_derive_key_509_50d(key);
	if (err == 0) {
		len = *(uint32_t *)(p + 0x80c);
		sm_memcpy(g_bigmac_src_buf, p + 0x08, len);
		bigmac_ctx_init_ch0(ctx);
		sm_memset(iv, 0, 0x10);
		if (bigmac_aes128_cbc_decrypt(g_bigmac_dst_buf, g_bigmac_src_buf, len, key, iv, ctx) == 0) {
			bigmac_wait_done(ctx);
			sm_memcpy(p + 0x08, g_bigmac_dst_buf, len);
		} else {
			err = 2;
		}
	}
	return err;
}

/* 0x80ccc2 */
int kirk_cmd_07_aes_cbc_decrypt(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint32_t key_id = *(uint32_t *)(p + 0x808);
	uint32_t len = *(uint32_t *)(p + 0x80c);
	const uint8_t *key_seed;
	uint32_t dst_slot, src_slot;
	int err;

	switch (key_id) {
	case 0x02: key_seed = g_keyseed_kirk_02; break;
	case 0x03: key_seed = g_keyseed_kirk_03; break;
	case 0x04: key_seed = g_keyseed_kirk_04; break;
	case 0x05: key_seed = g_keyseed_kirk_05; break;
	case 0x0c: key_seed = g_keyseed_kirk_0c; break;
	case 0x0d: key_seed = g_keyseed_kirk_0d; break;
	case 0x0e: key_seed = g_keyseed_kirk_0e; break;
	case 0x0f: key_seed = g_keyseed_kirk_0f; break;
	case 0x10: key_seed = g_keyseed_kirk_10; break;
	case 0x11: key_seed = g_keyseed_kirk_11; break;
	case 0x12: key_seed = g_keyseed_kirk_12; break;
	case 0x13: key_seed = g_keyseed_kirk_13_24_25; break;
	case 0x38: key_seed = g_keyseed_kirk_38; break;
	case 0x39: key_seed = g_keyseed_kirk_39; break;
	case 0x3a: key_seed = g_keyseed_kirk_3a; break;
	case 0x44: key_seed = g_keyseed_kirk_44; break;
	case 0x53: key_seed = g_keyseed_kirk_53; break;
	case 0x57: key_seed = g_keyseed_kirk_57; break;
	case 0x63: key_seed = g_keyseed_kirk_63; break;
	case 0x64: key_seed = g_keyseed_kirk_64; break;
	case 0x68: key_seed = g_keyseed_kirk_68_24; break;
	case 0x80: key_seed = g_keyseed_kirk_80; break;
	case 0x81: key_seed = g_keyseed_kirk_81; break;
	case 0x82: key_seed = g_keyseed_kirk_82; break;
	case 0x83: key_seed = g_keyseed_kirk_83; break;
	case 0xc0: key_seed = g_keyseed_kirk_c0; break;
	case 0xc1: key_seed = g_keyseed_kirk_c1; break;
	case 0xc2: key_seed = g_keyseed_kirk_c2; break;
	case 0xc3: key_seed = g_keyseed_kirk_c3; break;
	case 0x100: return kirk_cmd_07_decrypt_key100(req);
	default: return 0x0f;
	}

	if (key_id > 0x3au && (key_id - 0x80u > 0x3fu)) {
		dst_slot = 0x10u;
		src_slot = 0x340u;
	} else {
		dst_slot = 0x21u;
		src_slot = 0x345u;
	}

	err = (int)bigmac_derive_keyslot_ecb(key_seed, dst_slot, src_slot);
	if (err != 0)
		return err;
	sm_memcpy(g_bigmac_src_buf, p + 0x08, len);
	err = (int)bigmac_aes_cbc_keyslot(g_bigmac_dst_buf, g_bigmac_src_buf, len, dst_slot, 2u);
	if (err == 0)
		sm_memcpy(p + 0x08, g_bigmac_dst_buf, len);
	return err;
}

/* 0x80cf24 */
int kirk_cmd_04_encrypt_key100(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint32_t ctx[2];
	uint8_t iv[16];
	uint8_t key[16];
	uint32_t len;
	int err;

	err = kirk_derive_key_509_50d(key);
	if (err == 0) {
		len = *(uint32_t *)(p + 0x80c);
		sm_memcpy(g_bigmac_src_buf, p + 0x08, len);
		bigmac_ctx_init_ch0(ctx);
		sm_memset(iv, 0, 0x10);
		if (bigmac_aes128_cbc_encrypt(g_bigmac_dst_buf, g_bigmac_src_buf, len, key, iv, ctx) == 0) {
			bigmac_wait_done(ctx);
			sm_memcpy(p + 0x08, g_bigmac_dst_buf, len);
		} else {
			err = 2;
		}
	}
	return err;
}

/* 0x80cfbe */
int kirk_cmd_04_aes_cbc_encrypt(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint32_t key_id = *(uint32_t *)(p + 0x808);
	uint32_t len = *(uint32_t *)(p + 0x80c);
	const uint8_t *key_seed;
	int err;

	switch (key_id) {
	case 0x02: key_seed = g_keyseed_kirk_02; break;
	case 0x03: key_seed = g_keyseed_kirk_03; break;
	case 0x04: key_seed = g_keyseed_kirk_04; break;
	case 0x05: key_seed = g_keyseed_kirk_05; break;
	case 0x0c: key_seed = g_keyseed_kirk_0c; break;
	case 0x0d: key_seed = g_keyseed_kirk_0d; break;
	case 0x0e: key_seed = g_keyseed_kirk_0e; break;
	case 0x0f: key_seed = g_keyseed_kirk_0f; break;
	case 0x10: key_seed = g_keyseed_kirk_10; break;
	case 0x11: key_seed = g_keyseed_kirk_11; break;
	case 0x12: key_seed = g_keyseed_kirk_12; break;
	case 0x13: key_seed = g_keyseed_kirk_13_24_25; break;
	case 0x38: key_seed = g_keyseed_kirk_38; break;
	case 0x39: key_seed = g_keyseed_kirk_39; break;
	case 0x3a: key_seed = g_keyseed_kirk_3a; break;
	case 0x80: key_seed = g_keyseed_kirk_80; break;
	case 0x81: key_seed = g_keyseed_kirk_81; break;
	case 0x82: key_seed = g_keyseed_kirk_82; break;
	case 0x83: key_seed = g_keyseed_kirk_83; break;
	case 0x100: return kirk_cmd_04_encrypt_key100(req);
	default: return 0x0e;
	}

	err = (int)bigmac_derive_keyslot_ecb(key_seed, 0x21u, 0x345u);
	if (err == 0) {
		sm_memcpy(g_bigmac_src_buf, p + 0x08, len);
		err = (int)bigmac_aes_cbc_keyslot(g_bigmac_dst_buf, g_bigmac_src_buf, len, 0x21u, 1u);
		if (err == 0)
			sm_memcpy(p + 0x08, g_bigmac_dst_buf, len);
	}
	return err;
}

/* 0x80d13e */
uint32_t bigmac_aes_ecb_encrypt_slot0(void *dst, const void *src)
{
	if (src == NULL || dst == NULL)
		return 0x12u;
	MMIO_WRITE32(0xe0030028u, 0x212u);
	if (((MMIO_READ32(0xe003002cu) & 2u) >> 1) != 1u)
		return 0x11u;
	MMIO_WRITE32(0xe0050000u, MMIO_ADDPTR(src));
	MMIO_WRITE32(0xe0050004u, MMIO_ADDPTR(dst));
	MMIO_WRITE32(0xe0050008u, 0x10u);
	MMIO_WRITE32(0xe005000cu, 0x301u);
	MMIO_WRITE32(0xe0050010u, 0x212u);
	MMIO_WRITE32(0xe005001cu, 1u);
	while ((MMIO_READ32(0xe0050024u) & 1u) != 0)
		;
	return MMIO_READ32(0xe0050024u);
}

/* 0x80d1a8 */
int kirk_cmac_slot0_software(uint8_t *mac_out, const uint8_t *msg, uint32_t msg_len)
{
	uint32_t rem = msg_len & 0x0fu;
	uint32_t nblocks = msg_len >> 4;
	uint8_t last_blk[16];
	uint8_t subkey[16];
	int shifts, i, j, err;

	if (rem == 0) {
		if (msg_len != 0) {
			sm_memcpy(last_blk, msg + (msg_len - 0x10u), 0x10);
			shifts = 1;
		} else {
			nblocks = 1;
			last_blk[0] = 0x80u;
			sm_memset(last_blk + 1, 0, 0x0f);
			shifts = 2;
		}
	} else {
		nblocks++;
		sm_memcpy(last_blk, msg + (msg_len - rem), rem);
		last_blk[rem] = 0x80u;
		for (i = (int)rem + 1; i < 16; i++)
			last_blk[i] = 0;
		shifts = 2;
	}

	sm_memset(subkey, 0, 0x10);
	err = (int)bigmac_aes_ecb_encrypt_slot0(subkey, subkey);
	if (err == 0) {
		for (i = 0; i < shifts; i++) {
			uint8_t msb = subkey[0];
			for (j = 0; j < 15; j++)
				subkey[j] = (uint8_t)((subkey[j] << 1) | (subkey[j + 1] >> 7));
			subkey[15] <<= 1;
			if ((msb & 0x80u) != 0)
				subkey[15] ^= 0x87u;
		}
		sm_memset(mac_out, 0, 0x10);
		for (i = 0; i < (int)(nblocks - 1u); i++) {
			for (j = 0; j < 16; j++)
				mac_out[j] ^= msg[j];
			err = (int)bigmac_aes_ecb_encrypt_slot0(mac_out, mac_out);
			msg += 0x10;
			if (err != 0)
				return err;
		}
		for (j = 0; j < 16; j++)
			mac_out[j] ^= last_blk[j];
		for (j = 0; j < 16; j++)
			mac_out[j] ^= subkey[j];
		err = (int)bigmac_aes_ecb_encrypt_slot0(mac_out, mac_out);
	}
	return err;
}

/* 0x80d30a */
int kirk_cmd_19_verify_cmac_d8(void *req)
{
	uint8_t *p = (uint8_t *)req;
	int err;

	sm_memset(g_cmac_calc_buf, 0, 0x40);
	sm_memset(g_cmac_dec_buf, 0, 0x40);
	err = (int)bigmac_load_keyslot_204(g_keyseed_kirk_12_19);
	if (err == 0) {
		sm_memcpy(g_bigmac_src_buf, p + 0x08, 0xd8);
		err = kirk_cmac_slot0_software(g_cmac_calc_buf, g_bigmac_src_buf, 0xd8);
		if (err == 0) {
			sm_memcpy(g_bigmac_src_buf, p + 0xe0, 0x10);
			err = (int)bigmac_aes_ecb_slot0(g_cmac_dec_buf, g_bigmac_src_buf, 0x10);
			if (err == 0 && sm_memcmp_consttime(g_cmac_calc_buf, g_cmac_dec_buf, 0x10) != 0)
				err = 5;
		}
	}
	return err;
}

/* 0x80d39a */
int kirk_cmd_12_verify_cmac_a8(void *req)
{
	uint8_t *p = (uint8_t *)req;
	int err;

	sm_memset(g_cmac_calc_buf, 0, 0x40);
	sm_memset(g_cmac_dec_buf, 0, 0x40);
	err = (int)bigmac_load_keyslot_204(g_keyseed_kirk_12_19);
	if (err == 0) {
		sm_memcpy(g_bigmac_src_buf, p + 0x08, 0xa8);
		err = kirk_cmac_slot0_software(g_cmac_calc_buf, g_bigmac_src_buf, 0xa8);
		if (err == 0) {
			sm_memcpy(g_bigmac_src_buf, p + 0xb0, 0x10);
			err = (int)bigmac_aes_ecb_slot0(g_cmac_dec_buf, g_bigmac_src_buf, 0x10);
			if (err == 0 && sm_memcmp_consttime(g_cmac_calc_buf, g_cmac_dec_buf, 0x10) != 0)
				err = 5;
		}
	}
	return err;
}

/* 0x80d42a */
void keyring_read_slot_509(void *dst)
{
	uint32_t slot[8];

	keyring_read_slot(0x509u, slot);
	sm_memcpy(dst, slot, 0x10);
}

/* 0x80d456 */
void keyring_read_slot_50d(void *dst)
{
	uint32_t slot[8];

	keyring_read_slot(0x50du, slot);
	sm_memcpy(dst, slot, 0x10);
}

/* 0x80d482 */
void keyring_read_slot_600_rev(uint8_t *dst)
{
	uint32_t slot[8];
	const uint8_t *bytes = (const uint8_t *)slot;
	int i;

	keyring_read_slot(0x600u, slot);
	for (i = 0; i < 0x20; i++)
		dst[i] = bytes[0x1f - i];
}

/* 0x80d4b6 */
int kirk_cmd_24_gc_build_auth_pkt(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint8_t key_509[16];
	uint8_t key_600[32];
	uint8_t dec_509[16];
	uint8_t hdr[16];
	uint32_t ctx[2];
	uint32_t i;
	int err;

	keyring_read_slot_509(key_509);
	keyring_read_slot_600_rev(key_600);
	err = (int)bigmac_derive_keyslot_ecb(g_keyseed_kirk_68_24, 0x10u, 0x340u);
	if (err == 0) {
		sm_memcpy(g_bigmac_src_buf, key_509, 0x10);
		err = (int)bigmac_aes_cbc_keyslot(g_bigmac_dst_buf, g_bigmac_src_buf, 0x10u, 0x10u, 2u);
		if (err == 0) {
			sm_memcpy(dec_509, g_bigmac_dst_buf, 0x10);
			sm_memcpy(hdr, p + 0x08, 0x10);
			sm_memcpy(g_bigmac_src_buf, p + 0x18, 0x60);
			sm_memcpy(g_bigmac_src_buf + 0x20, dec_509, 0x10);
			g_bigmac_src_buf[0x58] = 0x10u;
			g_bigmac_src_buf[0x59] = 0x08u;
			g_bigmac_src_buf[0x5a] = key_600[2];
			hdr[1] = 0x01u;
			g_bigmac_src_buf[0x5b] = key_600[3];
			g_bigmac_src_buf[0x5c] = key_600[4];
			g_bigmac_src_buf[0x5d] = key_600[5];
			g_bigmac_src_buf[0x5e] = key_600[6];
			g_bigmac_src_buf[0x5f] = key_600[7];
			hdr[0] = 0x11u;
			sm_memset(hdr + 3, 0, 9);
			hdr[12] = 0;
			hdr[13] = 0;
			hdr[14] = 0;
			hdr[15] = 1;
			err = (int)bigmac_derive_keyslot_ecb(g_keyseed_kirk_13_24_25, 0x21u, 0x345u);
			if (err == 0) {
				for (i = 0; i < 0x60u; i++)
					g_bigmac_src_buf[i] ^= g_gc_req_iv_prefix[i & 0x0fu];
				err = (int)bigmac_aes_cbc_keyslot(g_bigmac_dst_buf, g_bigmac_src_buf, 0x60u, 0x21u, 1u);
				if (err == 0) {
					for (i = 0; i < 0x60u; i++)
						g_bigmac_dst_buf[i] ^= g_gc_req_iv_prefix[i & 0x0fu];
					sm_memcpy(p + 0x18, g_bigmac_dst_buf, 0x60);
					sm_memcpy(g_bigmac_src_buf, g_gc_req_iv_prefix, 0x10);
					sm_memcpy(g_bigmac_src_buf + 0x10, hdr, 0x10);
					sm_memcpy(g_bigmac_src_buf + 0x20, g_bigmac_dst_buf, 0x60);
					bigmac_ctx_init_ch0(ctx);
					if (bigmac_sha1_start(g_bigmac_dst_buf, g_bigmac_src_buf, 0x80u, ctx) == 0) {
						bigmac_wait_done(ctx);
						sm_memcpy(g_bigmac_src_buf, g_bigmac_dst_buf, 0x10);
						err = (int)bigmac_aes_cbc_keyslot(g_bigmac_dst_buf, g_bigmac_src_buf, 0x10u, 0x21u, 1u);
						if (err == 0) {
							sm_memcpy(p + 0x08, hdr, 0x10);
							sm_memcpy(p + 0x78, g_bigmac_dst_buf, 0x10);
						}
					} else {
						err = 0x11;
					}
				}
			}
		}
	}
	return err;
}

/* 0x80d6e0 */
int kirk_cmd_25_gc_verify_auth_resp(void *req)
{
	uint8_t *p = (uint8_t *)req;
	uint32_t ctx[2];
	uint8_t pkt[0x50];
	uint32_t i;
	int err;

	sm_memcpy(pkt, p + 0x08, 0x50);
	sm_memcpy(g_bigmac_src_buf, g_gc_resp_iv_prefix, 0x10);
	sm_memcpy(g_bigmac_src_buf + 0x10, pkt, 0x40);
	bigmac_ctx_init_ch0(ctx);
	if (bigmac_sha1_start(g_bigmac_dst_buf, g_bigmac_src_buf, 0x50u, ctx) == 0) {
		bigmac_wait_done(ctx);
		sm_memcpy(g_bigmac_src_buf, g_bigmac_dst_buf, 0x10);
		err = (int)bigmac_aes_cbc_keyslot(g_bigmac_dst_buf, g_bigmac_src_buf, 0x10u, 0x21u, 1u);
		if (err == 0) {
			if (sm_memcmp_consttime(g_bigmac_dst_buf, pkt + 0x40, 0x10) == 0) {
				err = (int)bigmac_derive_keyslot_ecb(g_keyseed_kirk_13_24_25, 0x21u, 0x345u);
				if (err == 0) {
					sm_memcpy(g_bigmac_src_buf, pkt + 0x10, 0x30);
					for (i = 0; i < 0x30u; i++)
						g_bigmac_src_buf[i] ^= g_gc_resp_iv_prefix[i & 0x0fu];
					err = (int)bigmac_aes_cbc_keyslot(g_bigmac_dst_buf, g_bigmac_src_buf, 0x30u, 0x21u, 2u);
					if (err == 0) {
						for (i = 0; i < 0x30u; i++)
							g_bigmac_dst_buf[i] ^= g_gc_resp_iv_prefix[i & 0x0fu];
						sm_memcpy(p + 0x08, g_bigmac_dst_buf, 0x30);
					}
				}
			} else {
				err = 5;
			}
		}
	} else {
		err = 0x11;
	}
	return err;
}

/* 0x80d826 */
void kirk_service_dispatch(void *pkt)
{
	uint8_t *p = (uint8_t *)pkt;
	void *req = p + 0x40;
	int res;

	res = kirk_validate_req_size(req);
	if (res == 0) {
		if (*(uint32_t *)(p + 0x40) == 1u) {
			switch (*(uint32_t *)(p + 0x44)) {
			case 0x04: res = kirk_cmd_04_aes_cbc_encrypt(req); break;
			case 0x07: res = kirk_cmd_07_aes_cbc_decrypt(req); break;
			case 0x0c: res = kirk_cmd_0c_ecdsa_keygen_160(req); break;
			case 0x0d: res = kirk_cmd_0d_ec_mul_160(req); break;
			case 0x0e: res = kirk_cmd_0e_rng_20(req); break;
			case 0x10: res = kirk_cmd_10_ecdsa_sign_160(req); break;
			case 0x21: res = kirk_cmd_21_ecdsa_sign_160_det(req); break;
			case 0x11: res = kirk_cmd_11_ecdsa_verify_160(req); break;
			case 0x12: res = kirk_cmd_12_verify_cmac_a8(req); break;
			case 0x14: res = kirk_cmd_14_ecdsa_keygen_224(req); break;
			case 0x15: res = kirk_cmd_15_ec_mul_224(req); break;
			case 0x16: res = kirk_cmd_16_rng_28(req); break;
			case 0x17: res = kirk_cmd_17_ecdsa_sign_224(req); break;
			case 0x22: res = kirk_cmd_22_ecdsa_sign_224_det(req); break;
			case 0x18: res = kirk_cmd_18_ecdsa_verify_224(req); break;
			case 0x19: res = kirk_cmd_19_verify_cmac_d8(req); break;
			case 0x1b: res = kirk_cmd_1b_verify_challenge(req); break;
			case 0x1c: res = kirk_cmd_1c_gen_token_a3(req); break;
			case 0x1d: res = kirk_cmd_1d_verify_cert_block(req); break;
			case 0x1e: res = kirk_cmd_1e_gen_challenge_b1(req); break;
			case 0x1f: res = kirk_cmd_1f_decrypt_session_block(req); break;
			case 0x20: res = kirk_cmd_20_verify_key_blocks(req); break;
			case 0x23: res = kirk_cmd_23_encrypt_and_cmac(req); break;
			case 0x24: res = kirk_cmd_24_gc_build_auth_pkt(req); break;
			case 0x25: res = kirk_cmd_25_gc_verify_auth_resp(req); break;
			default:   res = 2; break;
			}
		} else {
			res = 1;
		}
	}
	*(int *)(p + 0x08) = res;
}

/* 0x80d9ac */
int sm_validate_phys_range(uint32_t addr, uint32_t size)
{
	uint32_t dram_size = ((uint32_t)MMIO_READ8(0xe0062263u) << 24) |
			     ((uint32_t)MMIO_READ8(0xe0062262u) << 16) |
			     ((uint32_t)MMIO_READ8(0xe0062261u) << 8) |
			     (uint32_t)MMIO_READ8(0xe0062260u);
	uint32_t dram_end = dram_size + 0x40000000u;
	int carry_dram = (dram_end < dram_size);
	uint32_t end = addr + size;
	int carry_end = (end < size);

	if (addr >= 0x40200000u) {
		if (carry_dram || addr < dram_end) {
			if (!((carry_dram < carry_end) || (carry_end == carry_dram && dram_end < end)))
				return 1;
		}
	} else if (addr < 0x1f000000u) {
		return 0;
	}

	if (addr <= 0x1f007fffu && end >= size && end <= 0x1f008000u)
		return 1;
	if (addr >= 0x1f840000u && addr < 0x1f860000u) {
		if (end >= size && end <= 0x1f860000u)
			return 1;
		return 0;
	}
	return 0;
}

/* 0x80da80 */
void sm_mailbox_irq_handler(void)
{
	uint32_t reg = MMIO_READ32(0xe0000014u);
	uint32_t phys_addr;
	uint32_t pkt_size;
	uint32_t resp = 4u;

	if ((reg & 1u) == 0)
		return;
	MMIO_WRITE32(0xe0000014u, reg);
	phys_addr = reg & ~1u;
	if (sm_validate_phys_range(phys_addr, 0x40u) != 0) {
		pkt_size = MMIO_READ32(phys_addr);
		if (pkt_size >= 0x40u && pkt_size <= 0x1000u && sm_validate_phys_range(phys_addr, pkt_size) != 0) {
			if (bigmac_dma_memcpy(g_mailbox_pkt_buf, MMIO_RAW_PTR(phys_addr), pkt_size) != 0) {
				resp = 8u;
			} else if (g_mailbox_pkt_buf[0] == pkt_size) {
				uint32_t svc_id = g_mailbox_pkt_buf[1];
				if (svc_id == 0xffffffffu) {
					g_sm_srv_ctx_ptr[0] = 0;
					return;
				}
				uintptr_t cur = g_sm_srv_ctx_ptr[1];
				resp = 2u;
				while (cur != 0) {
					uintptr_t *node = (uintptr_t *)(cur & ~(uintptr_t)1u);
					if ((uint32_t)node[0] == svc_id) {
						void (**vtable)(void *, void *) =
							(void (**)(void *, void *))node[2];
						vtable[0](node + 2, g_mailbox_pkt_buf);
						if (bigmac_dma_memcpy(MMIO_RAW_PTR(phys_addr),
							       g_mailbox_pkt_buf, pkt_size) != 0) {
							resp = 8u;
						} else {
							sm_mem_barrier();
							resp = 0u;
						}
						break;
					}
					cur = node[1];
				}
			}
		}
	}
	MMIO_WRITE32(0xe0000004u, resp | 1u);
}

/* 0x80db4e */
int sm_srv_init_zero(uintptr_t *ctx)
{
	ctx[0] = 0;
	ctx[1] = 0;
	ctx[2] = 0;
	ctx[3] = (uintptr_t)(ctx + 5);
	g_sm_srv_ctx_ptr = ctx;
	ctx[4] = 0x60u * sizeof(uintptr_t);
	return 0;
}

/* 0x80db70 */
void sm_srv_init(uintptr_t *ctx)
{
	ctx[0] = 0;
	ctx[1] = 0;
	ctx[2] = 0;
	ctx[3] = (uintptr_t)(ctx + 5);
	g_sm_srv_ctx_ptr = ctx;
	ctx[4] = 0x60u * sizeof(uintptr_t);
}

/* 0x80db92 */
uintptr_t *sm_srv_get_ctx(void)
{
	return g_sm_srv_ctx_ptr;
}

/* 0x80db98 */
uintptr_t *sm_srv_alloc_node(uintptr_t *ctx)
{
	uintptr_t cur = ctx[3];
	uintptr_t end = cur + ctx[4];
	uint32_t idx = 0;

	while (cur < end) {
		uintptr_t mask = (uintptr_t)1u << (idx & 0x1fu);
		if ((ctx[2] & mask) == 0) {
			ctx[2] |= mask;
			return (uintptr_t *)cur;
		}
		idx++;
		cur += 6u * sizeof(uintptr_t);
	}
	return NULL;
}

/* 0x80dbcc */
int sm_srv_free_node(uintptr_t *ctx, uintptr_t *node)
{
	uintptr_t base = ctx[3];
	uintptr_t addr = (uintptr_t)node;

	if (addr >= base && addr < base + ctx[4]) {
		uint32_t idx = (uint32_t)sm_divsi3((int32_t)(addr - base), (int32_t)(6u * sizeof(uintptr_t)));
		ctx[2] &= ~((uintptr_t)1u << (idx & 0x1fu));
		return 0;
	}
	return -1;
}

/* 0x80dc10 */
void sm_srv_teardown(uintptr_t *ctx)
{
	while (ctx[1] != 0)
		sm_srv_unlink_node(ctx, (uintptr_t *)ctx[1]);
	g_sm_srv_ctx_ptr = NULL;
}

/* 0x80dc34 */
void sm_srv_teardown_all(uintptr_t *ctx)
{
	while (ctx[1] != 0)
		sm_srv_unlink_node(ctx, (uintptr_t *)ctx[1]);
	g_sm_srv_ctx_ptr = NULL;
}

/* 0x80dc58 */
void sm_srv_run_loop(uintptr_t *ctx)
{
	uint32_t prev_handler = 0;

	ctx[0] = 1u;
	if (sm_syscall_register_irq(9u, sm_mailbox_irq_handler, &prev_handler) == 1u) {
		while (*(volatile uintptr_t *)ctx == 1u) {
#ifdef SM_HOST_TEST
			sm_mailbox_irq_handler();
#endif
		}
		sm_syscall_register_irq(9u, NULL, &prev_handler);
	}
}

/* 0x80dc98 */
uint32_t sm_srv_unlink_node(uintptr_t *ctx, uintptr_t *node)
{
	uintptr_t *prev_next = &ctx[1];
	uintptr_t cur = ctx[1];

	node[1] |= 1u;
	while (cur != 0) {
		uintptr_t *ent = (uintptr_t *)(cur & ~(uintptr_t)1u);
		uintptr_t next = ent[1];
		if ((next & 1u) != 0) {
			next &= ~(uintptr_t)1u;
			if (ent == node) {
				*prev_next = next;
				sm_srv_free_node(ctx, node);
				return 0;
			}
			cur = next;
		} else {
			prev_next = &ent[1];
			cur = next;
		}
	}
	return 0x800f0016u;
}

/* 0x80dce8 */
void sm_mem_barrier(void)
{
	cmep_synm();
}

/* 0x80dcec */
int bigmac_ctx_init_ch0(uint32_t *ctx)
{
	ctx[0] = 0;
	ctx[1] = 0x2080u;
	return 0;
}

/* 0x80dcfc */
int bigmac_ctx_get_status(const uint32_t *ctx, uint32_t *status_out)
{
	uint32_t ch_off = (ctx[0] & 1u) * 0x80u;

	*status_out = MMIO_READ32(0xe0050024u + ch_off);
	return 0;
}

/* 0x80dd16 */
uint32_t bigmac_ctx_wait(const uint32_t *ctx)
{
	uint32_t ch_off = (ctx[0] & 1u) * 0x80u;

	while ((MMIO_READ32(0xe0050024u + ch_off) & 1u) != 0)
		;
	if (MMIO_READ32(0xe0050024u + ch_off) == 0)
		return 0;
	return 0x800f0005u;
}

/* 0x80dd44 */
uint32_t bigmac_aes128_cbc_encrypt(void *dst, const void *src, uint32_t len, const void *key, const void *iv, uint32_t *ctx)
{
	return bigmac_cipher_start(dst, src, len, key, iv, 1u, 1u, 1u, ctx);
}

/* 0x80dd66 */
uint32_t bigmac_aes128_cbc_decrypt(void *dst, const void *src, uint32_t len, const void *key, const void *iv, uint32_t *ctx)
{
	return bigmac_cipher_start(dst, src, len, key, iv, 2u, 1u, 1u, ctx);
}

/* 0x80dd8a */
uint32_t bigmac_sha1_start(void *dst, const void *src, uint32_t len, uint32_t *ctx)
{
	return bigmac_hash_start(dst, src, len, 0u, ctx);
}

/* 0x80dd9e */
uint32_t bigmac_sha256_start(void *dst, const void *src, uint32_t len, uint32_t *ctx)
{
	return bigmac_hash_start(dst, src, len, 2u, ctx);
}

/* 0x80ddb2 */
uint32_t bigmac_hmac_sha256_start(void *dst, const void *src, uint32_t len, const void *key, uint32_t key_len, uint32_t *ctx)
{
	return bigmac_hmac_start(dst, src, len, key, key_len, 6u, ctx);
}

/* 0x80ddce */
uint32_t bigmac_aes128_cmac_inline(void *dst, const void *src, uint32_t len, const void *key, uint32_t key_len, uint32_t *ctx)
{
	uint32_t flags = ctx[1];
	uint32_t ch_off, mode, i;
	const uint32_t *kwords = (const uint32_t *)key;

	if ((key_len != 0x10u && key_len != 0x18u && key_len != 0x20u) ||
	    len == 0 || (len & 0x0fu) != 0)
		return 0x800f0016u;

	ch_off = (ctx[0] & 1u) * 0x80u;
	MMIO_WRITE32(0xe0050000u + ch_off, MMIO_ADDPTR(src));
	MMIO_WRITE32(0xe0050004u + ch_off, MMIO_ADDPTR(dst));
	MMIO_WRITE32(0xe0050008u + ch_off, len);
	mode = (flags & 0xfffffff8u) | 0x3bu;
	if (key_len == 0x18u)
		mode = (flags & 0xfffffcf8u) | 0x23bu;
	else if (key_len == 0x20u)
		mode = (flags & 0xfffffff8u) | 0x33bu;
	else if (key_len == 0x10u)
		mode = (flags & 0xfffffcf8u) | 0x13bu;

	MMIO_WRITE32(0xe005000cu + ch_off, mode & 0xfffff3ffu);
	if ((mode & 0x80u) == 0) {
		MMIO_WRITE32(0xe0050010u + ch_off, kwords[0]);
	} else {
		for (i = 0; i < (key_len >> 2); i++)
			MMIO_WRITE32(0xe0050200u + ch_off + i * 4u, kwords[i]);
	}
	MMIO_WRITE32(0xe005001cu + ch_off, 1u);
	return 0;
}

/* 0x80dec4 */
uint32_t bigmac_dma_memcpy(void *dst, const void *src, uint32_t len)
{
	uint32_t ctx[2];
	uint32_t err;

	bigmac_ctx_init_ch0(ctx);
	err = bigmac_memcpy_start(dst, src, len, ctx);
	if (err == 0)
		err = bigmac_ctx_wait(ctx);
	return err;
}

/* 0x80defc */
uint32_t bigmac_cipher_start(void *dst, const void *src, uint32_t len, const void *key, const void *iv,
		    uint32_t op, uint32_t mode, uint32_t key_idx, uint32_t *ctx)
{
	uint32_t ch = ctx[0];
	uint32_t flags = ctx[1];
	uint32_t key_sizes[4];
	uint32_t ch_off, klen, i;
	const uint32_t *kwords = (const uint32_t *)key;

	sm_memcpy(key_sizes, g_bigmac_key_sizes, 0x10);
	if (((MMIO_ADDPTR(dst) & 3u) != 0) ||
	    (((MMIO_ADDPTR(iv) | MMIO_ADDPTR(src)) & 3u) != 0))
		return 0x800f0016u;

	ch_off = (ch & 1u) * 0x80u;
	MMIO_WRITE32(0xe0050000u + ch_off, MMIO_ADDPTR(src));
	MMIO_WRITE32(0xe0050004u + ch_off, MMIO_ADDPTR(dst));
	MMIO_WRITE32(0xe0050008u + ch_off, len);
	MMIO_WRITE32(0xe005000cu + ch_off,
		     (flags & 0xfffffcc0u) | ((key_idx & 3u) << 8) | ((mode & 7u) << 3) | (op & 7u));
	if (iv != NULL)
		MMIO_WRITE32(0xe0050014u + ch_off, MMIO_ADDPTR(iv));
	if ((flags & 0x80u) == 0) {
		MMIO_WRITE32(0xe0050010u + ch_off, kwords[0]);
	} else {
		klen = key_sizes[key_idx];
		for (i = 0; i < (klen >> 2); i++)
			MMIO_WRITE32(0xe0050200u + ch_off + i * 4u, kwords[i]);
	}
	MMIO_WRITE32(0xe005001cu + ch_off, 1u);
	return 0;
}

/* 0x80e016 */
uint32_t bigmac_hash_start(void *dst, const void *src, uint32_t len, uint32_t hash_mode, uint32_t *ctx)
{
	uint32_t flags = ctx[1];
	uint32_t ch_off;

	if ((MMIO_ADDPTR(src) & 3u) != 0)
		return 0x800f0016u;
	ch_off = (ctx[0] & 1u) * 0x80u;
	MMIO_WRITE32(0xe0050000u + ch_off, MMIO_ADDPTR(src));
	MMIO_WRITE32(0xe0050004u + ch_off, MMIO_ADDPTR(dst));
	MMIO_WRITE32(0xe0050008u + ch_off, len);
	MMIO_WRITE32(0xe005000cu + ch_off, (flags & 0xfffff3c0u) | ((hash_mode & 7u) << 3) | 3u);
	MMIO_WRITE32(0xe005001cu + ch_off, 1u);
	return 0;
}

/* 0x80e080 */
uint32_t bigmac_hmac_start(void *dst, const void *src, uint32_t len, const void *key, uint32_t key_len,
		    uint32_t hmac_mode, uint32_t *ctx)
{
	uint32_t flags = ctx[1];
	uint32_t ch_off, i;
	const uint32_t *kwords = (const uint32_t *)key;

	if (key_len != 0x40u || (MMIO_ADDPTR(src) & 3u) != 0)
		return 0x800f0016u;
	ch_off = (ctx[0] & 1u) * 0x80u;
	MMIO_WRITE32(0xe0050000u + ch_off, MMIO_ADDPTR(src));
	MMIO_WRITE32(0xe0050004u + ch_off, MMIO_ADDPTR(dst));
	MMIO_WRITE32(0xe0050008u + ch_off, len);
	MMIO_WRITE32(0xe005000cu + ch_off, (flags & 0xfffff3c0u) | ((hmac_mode & 7u) << 3) | 3u);
	for (i = 0; i < 0x10u; i++)
		MMIO_WRITE32(0xe0050200u + ch_off + i * 4u, kwords[i]);
	MMIO_WRITE32(0xe005001cu + ch_off, 1u);
	return 0;
}

/* 0x80e102 */
uint32_t bigmac_memcpy_start(void *dst, const void *src, uint32_t len, uint32_t *ctx)
{
	uint32_t flags = ctx[1];
	uint32_t ch_off;

	if (len - 1u >= 0xffffffu)
		return 0x800f0016u;
	ch_off = (ctx[0] & 1u) * 0x80u;
	MMIO_WRITE32(0xe0050000u + ch_off, MMIO_ADDPTR(src));
	MMIO_WRITE32(0xe0050004u + ch_off, MMIO_ADDPTR(dst));
	MMIO_WRITE32(0xe0050008u + ch_off, len);
	MMIO_WRITE32(0xe005000cu + ch_off, flags & 0xfffffff8u);
	MMIO_WRITE32(0xe005001cu + ch_off, 1u);
	return 0;
}

/* 0x80e164..0x80e18e */
uint32_t sm_syscall_unload(uint32_t a1, uint32_t a2, uint32_t a3)
{
	return cmep_syscall(1u, a1, a2, a3);
}

uint32_t sm_syscall_2(uint32_t a1, uint32_t a2, uint32_t a3)
{
	return cmep_syscall(2u, a1, a2, a3);
}

uint32_t sm_syscall_3(uint32_t a1, uint32_t a2, uint32_t a3)
{
	return cmep_syscall(3u, a1, a2, a3);
}

uint32_t sm_syscall_register_irq(uint32_t a1, void (*a2)(void), uint32_t *a3)
{
	return cmep_syscall(4u, a1, (uint32_t)(uintptr_t)a2, (uint32_t)(uintptr_t)a3);
}

uint32_t sm_syscall_5(uint32_t a1, uint32_t a2, uint32_t a3)
{
	return cmep_syscall(5u, a1, a2, a3);
}

uint32_t sm_syscall_6(uint32_t a1, uint32_t a2, uint32_t a3)
{
	return cmep_syscall(6u, a1, a2, a3);
}

uint32_t sm_syscall_panic(uint32_t a1)
{
	return cmep_syscall(7u, a1, 0u, 0u);
}

uint32_t sm_syscall_8(uint32_t a1, uint32_t a2, uint32_t a3)
{
	return cmep_syscall(8u, a1, a2, a3);
}

/* 0x80e194..0x80e1ce */
void cop_rng_init(void)
{
	cop_rng_wait();
	cmep_stcb(0x413u, 0x20u);
	cmep_stcb(0x415u, 5u);
	cmep_stcb(0x410u, 1u);
	cmep_stcb(0x411u, 0u);
	cmep_stcb(0x412u, 1u);
}

void cop_rng_wait(void)
{
	if (cmep_ldcb(0x412u) != 0)
		cmep_stcb(0x412u, 0u);
}

uint32_t cop_rng_read(void)
{
	return cmep_ldcb(0x410u);
}

uint32_t cop_rng_get_word(void)
{
	return cmep_ldcb(0x415u);
}

/* Helper to poll Bignum status register 0xe0040804 */
static inline uint32_t bignum_wait(void)
{
	uint32_t st;

	do {
		st = MMIO_READ32(0xe0040804u);
		if ((int32_t)st >= 0)
			break;
	} while ((st & 0x04000000u) == 0);
	return st;
}

static inline void bignum_exec(uint32_t cmd)
{
	MMIO_WRITE32(0xe0040800u, cmd);
	bignum_wait();
}

static int bignum_top_word(const uint32_t *w, uint32_t nwords)
{
	int idx = (int)nwords - 1;

	while (idx >= 0 && w[idx] == 0)
		idx--;
	return idx;
}

/* 0x80e1d4 */
int ec_pubkey_from_priv(uint8_t **pub_xy, const uint8_t *priv, const uint8_t * const *curve,
	       uint32_t nwords, uint32_t nbytes)
{
	uint32_t p[7], a[7], n[7], k[7];
	uint32_t gx[7], gy[7], gz[7], ox[7], oy[7], oz[7];
	uint32_t *in_xy[3] = { gx, gy, gz };
	uint32_t *out_xy[3] = { ox, oy, oz };

	bignum_be_to_le_words(p, curve[0], (int)nbytes);
	bignum_be_to_le_words(a, curve[1], (int)nbytes);
	bignum_be_to_le_words(n, curve[3], (int)nbytes);
	bignum_be_to_le_words(gx, curve[4], (int)nbytes);
	bignum_be_to_le_words(gy, curve[5], (int)nbytes);
	bignum_be_to_le_words(k, priv, (int)nbytes);
	if (!bignum_is_valid_scalar(k, n, (int)nwords))
		return -1;
	ec_scalar_mul_montgomery(out_xy, in_xy, p, a, k, nwords);
	bignum_le_words_to_be(pub_xy[0], out_xy[0], (int)nwords);
	bignum_le_words_to_be(pub_xy[1], out_xy[1], (int)nwords);
	return 0;
}

/* 0x80e2e2 */
int ec_pubkey_from_priv_160(uint8_t **pub_xy, const uint8_t *priv, const uint8_t * const *curve)
{
	return ec_pubkey_from_priv(pub_xy, priv, curve, 5u, 0x14u);
}

/* 0x80e2f6 */
int ec_pubkey_from_priv_224(uint8_t **pub_xy, const uint8_t *priv, const uint8_t * const *curve)
{
	return ec_pubkey_from_priv(pub_xy, priv, curve, 7u, 0x1cu);
}

/* 0x80e30a */
int bignum_is_valid_scalar(const uint32_t *val, const uint32_t *limit, int nwords)
{
	if (bignum_top_word(val, (uint32_t)nwords) >= 0)
		return bignum_cmp_gt(limit, val, nwords) == 1;
	return 0;
}

static uint32_t count_leading_zeros_u32(uint32_t x)
{
	uint32_t s16 = (uint32_t)((int32_t)~(x >> 16) >> 16) & 16u;
	uint32_t v = x >> s16;
	uint32_t s8 = (uint32_t)((int32_t)(v - 0x100u) >> 16) & 8u;
	v <<= s8;
	uint32_t s4 = (uint32_t)((int32_t)(v - 0x1000u) >> 16) & 4u;
	v <<= s4;
	uint32_t s2 = (uint32_t)((int32_t)(v - 0x4000u) >> 16) & 2u;
	uint32_t top2 = (v << s2) >> 14;
	return (18u - s16 + s8 + s4 + s2) - (~((int32_t)top2 >> 1) & top2);
}

/* 0x80e356 */
void ec_scalar_mul_montgomery(uint32_t **out_xy, uint32_t **in_xy, const uint32_t *p, const uint32_t *a,
		const uint32_t *scalar, uint32_t nwords)
{
	uint32_t p_words = nwords;
	uint32_t i, wcmd, mcmd;
	int pt_nonzero = 0;

	if (nwords - 1u > 6u)
		return;
	while (p_words > 0 && p[p_words - 1u] == 0)
		p_words--;
	if (p_words == 0)
		return;

	MMIO_WRITE32(0xe0040000u + p_words * 8u, 1u);
	bignum_exec(((p_words & 0x7fu) << 19) | 0xe0000000u);
	for (i = 0; i < p_words; i++)
		MMIO_WRITE32(0xe0040400u + i * 4u, p[i]);
	bignum_exec(((p_words & 0x1ffu) << 9) | 0x58000000u | (((p_words * 2u + 1u) << 18) & 0x03fc0000u));
	wcmd = (p_words & 0xffu) << 18;
	mcmd = wcmd | 0x80000000u;
	bignum_exec(wcmd | 0xc0000042u);
	bignum_exec(0x78000000u);

	for (i = 0; i < p_words; i++) {
		if (in_xy[0][i] != 0 || in_xy[1][i] != 0)
			pt_nonzero = 1;
	}
	if (!pt_nonzero) {
		bignum_exec(0xe01cf400u);
	} else {
		for (i = 0; i < p_words; i++)
			MMIO_WRITE32(0xe0040508u + i * 4u, in_xy[0][i]);
		bignum_exec(mcmd);
		for (i = 0; i < p_words; i++)
			MMIO_WRITE32(0xe00401b0u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
		bignum_exec(wcmd | 0xc0028515u);

		for (i = 0; i < p_words; i++)
			MMIO_WRITE32(0xe0040508u + i * 4u, in_xy[1][i]);
		bignum_exec(mcmd);
		for (i = 0; i < p_words; i++)
			MMIO_WRITE32(0xe00401ccu + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
		bignum_exec(wcmd | 0xc002851cu);

		MMIO_WRITE32(0xe0040508u, 1u);
		bignum_exec((((p_words - 1u) << 18) & 0x03fc0000u) | 0xe0028600u);
		bignum_exec(mcmd);
		for (i = 0; i < p_words; i++)
			MMIO_WRITE32(0xe00401e8u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
		bignum_exec(wcmd | 0xc0028523u);

		for (i = 0; i < p_words; i++)
			MMIO_WRITE32(0xe0040508u + i * 4u, a[i]);
		bignum_exec(mcmd);
		for (i = 0; i < p_words; i++)
			MMIO_WRITE32(0xe0040194u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));

		ec_point_add_jacobian(p_words);
		for (i = 0; i < p_words; i++) {
			MMIO_WRITE32(0xe00405b0u + i * 4u, MMIO_READ32(0xe00401b0u + i * 4u));
			MMIO_WRITE32(0xe00405ccu + i * 4u, MMIO_READ32(0xe00401ccu + i * 4u));
			MMIO_WRITE32(0xe00405e8u + i * 4u, MMIO_READ32(0xe00401e8u + i * 4u));
		}
		ec_jacobian_to_affine_step(MMIO_PTR(0xe0040454u), 0x15u, p_words);
		for (i = 0; i < p_words; i++) {
			MMIO_WRITE32(0xe00404a8u + i * 4u, MMIO_READ32(0xe00401b0u + i * 4u));
			MMIO_WRITE32(0xe00404c4u + i * 4u, MMIO_READ32(0xe00401ccu + i * 4u));
			MMIO_WRITE32(0xe00404e0u + i * 4u, MMIO_READ32(0xe00401e8u + i * 4u));
		}
		ec_jacobian_to_affine_step(MMIO_PTR(0xe00405b0u), 0x6cu, p_words);
		for (i = 0; i < p_words; i++) {
			MMIO_WRITE32(0xe004055cu + i * 4u, MMIO_READ32(0xe00401b0u + i * 4u));
			MMIO_WRITE32(0xe0040578u + i * 4u, MMIO_READ32(0xe00401ccu + i * 4u));
			MMIO_WRITE32(0xe0040594u + i * 4u, MMIO_READ32(0xe00401e8u + i * 4u));
		}
		ec_jacobian_to_affine_step(MMIO_PTR(0xe00405b0u), 0x6cu, p_words);
		for (i = 0; i < p_words; i++) {
			MMIO_WRITE32(0xe00405b0u + i * 4u, MMIO_READ32(0xe00401b0u + i * 4u));
			MMIO_WRITE32(0xe00405ccu + i * 4u, MMIO_READ32(0xe00401ccu + i * 4u));
			MMIO_WRITE32(0xe00405e8u + i * 4u, MMIO_READ32(0xe00401e8u + i * 4u));
		}

		uint32_t clz_total = 0;
		for (i = 0; i < nwords; i++) {
			uint32_t c = count_leading_zeros_u32(scalar[nwords - 1u - i]);
			clz_total += c;
			if (c != 32u)
				break;
		}
		int rem_words = (int)(nwords - (clz_total >> 5));
		uint32_t shifted[9] = { 0 };
		uint32_t *sp_ptr = shifted;
		uint32_t bit_off = clz_total & 0x1fu;
		bignum_shl_words(sp_ptr, scalar, rem_words, bit_off);
		int top_idx = rem_words - 1;
		bignum_exec(0xe054d800u);
		MMIO_WRITE32(0xe00401b0u, 1u);
		MMIO_WRITE32(0xe00401ccu, 1u);

		if (rem_words > 0) {
			int rem_bits = (int)(nwords * 32u - clz_total);
			int init_dbl = 4;
			for (;;) {
				uint32_t win = (shifted[top_idx] >> 29) & 3u;
				int wshift = (int)g_ec_shamir_win_table[win].shift;
				for (; init_dbl < wshift; init_dbl++)
					ec_point_add_jacobian(p_words);
				ec_jacobian_to_affine_step(MMIO_PTR(g_ec_shamir_win_table[win].reg_addr),
					   g_ec_shamir_win_table[win].reg_idx, p_words);
				bignum_shl_words(sp_ptr, sp_ptr, rem_words, (uint32_t)wshift);
				bit_off += (uint32_t)wshift;
				rem_bits -= wshift;
				if (bit_off > 31u) {
					sp_ptr++;
					bit_off &= 0x1fu;
					rem_words--;
				}
				int zero_run = 0;
				while ((int32_t)shifted[top_idx] >= 0) {
					if (rem_bits < 1)
						break;
					bignum_shl_words(sp_ptr, sp_ptr, rem_words, 1u);
					bit_off++;
					zero_run++;
					rem_bits--;
					if (bit_off == 32u) {
						sp_ptr++;
						rem_words--;
						bit_off = 0;
						if ((int32_t)shifted[top_idx] < 0)
							break;
					}
				}
				for (; zero_run != 0; zero_run--)
					ec_point_add_jacobian(p_words);
				if (rem_words < 1)
					break;
				init_dbl = 0;
			}
		}
	}

	int z_nonzero = 0;
	for (i = 0; i < p_words; i++) {
		if (MMIO_READ32(0xe00401e8u + i * 4u) != 0)
			z_nonzero = 1;
	}
	if (!z_nonzero) {
		for (i = 0; i < p_words; i++) {
			out_xy[0][i] = 0;
			out_xy[1][i] = 0;
		}
		return;
	}

	bignum_exec(wcmd | 0xc000d82au);
	bignum_exec(wcmd | 0xc000e631u);
	for (i = 0; i < p_words; i++) {
		MMIO_WRITE32(0xe0040108u + i * 4u, p[i]);
		MMIO_WRITE32(0xe0040508u + i * 4u, MMIO_READ32(0xe00401e8u + i * 4u));
	}
	bignum_exec((((p_words + 0x142u) << 9) & 0x0003fe00u) | 0xe0040000u);
	bignum_exec(wcmd | 0xb0000000u);
	bignum_exec(wcmd | 0xb8000000u);
	for (i = 0; i < p_words; i++)
		MMIO_WRITE32(0xe0040400u + i * 4u, p[i]);
	bignum_exec(wcmd | 0xc000546cu);
	bignum_exec(wcmd | 0xc0006273u);
	bignum_exec(wcmd | 0xc0000042u);
	MMIO_WRITE32(0xe0040508u, 1u);
	bignum_exec((((p_words - 1u) & 0xffu) << 18) | 0xe0028600u);
	bignum_exec(mcmd);
	bignum_exec(mcmd);
	bignum_exec(wcmd | 0xc0028549u);
	bignum_exec(mcmd);
	bignum_exec(wcmd | 0xc000e642u);
	bignum_exec(mcmd);
	for (i = 0; i < p_words; i++)
		out_xy[1][i] = MMIO_READ32(0xe0040508u + i * 4u);
	bignum_exec(wcmd | 0xc000d842u);
	bignum_exec(wcmd | 0xc0029342u);
	bignum_exec(mcmd);
	for (i = 0; i < p_words; i++)
		out_xy[0][i] = MMIO_READ32(0xe0040508u + i * 4u);
	for (i = p_words; i < nwords; i++) {
		out_xy[0][i] = 0;
		out_xy[1][i] = 0;
	}
}

/* 0x80ef8e */
int ec_point_mul(uint8_t **out_xy, const uint8_t *scalar, uint8_t **in_xy,
	       const uint8_t * const *curve, uint32_t nwords, uint32_t nbytes)
{
	uint32_t p[7], a[7], n[7], k[7];
	uint32_t ix[7], iy[7], iz[7], ox[7], oy[7], oz[7];
	uint32_t *pt_in[3] = { ix, iy, iz };
	uint32_t *pt_out[3] = { ox, oy, oz };
	uint32_t p_words = nwords, i, cmd;

	bignum_be_to_le_words(p, curve[0], (int)nbytes);
	bignum_be_to_le_words(a, curve[1], (int)nbytes);
	bignum_be_to_le_words(n, curve[3], (int)nbytes);
	bignum_be_to_le_words(ix, in_xy[0], (int)nbytes);
	bignum_be_to_le_words(iy, in_xy[1], (int)nbytes);

	while (p_words > 0 && p[p_words - 1u] == 0)
		p_words--;
	if (bignum_top_word(p, p_words) < 0)
		return -1;

	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040000u + i * 4u, ix[i]);
	for (i = 0; i < p_words; i++)
		MMIO_WRITE32(0xe0040400u + i * 4u, p[i]);
	cmd = ((nwords & 0xffu) << 18) | 0x58000000u | ((p_words & 0x1ffu) << 9);
	bignum_exec(cmd);
	for (i = 0; i < p_words; i++)
		ix[i] = MMIO_READ32(0xe0040000u + i * 4u);

	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040000u + i * 4u, iy[i]);
	bignum_exec(cmd);
	for (i = 0; i < p_words; i++)
		iy[i] = MMIO_READ32(0xe0040000u + i * 4u);

	bignum_be_to_le_words(k, scalar, (int)nbytes);
	if (!bignum_is_valid_scalar(k, n, (int)nwords))
		return -1;
	ec_scalar_mul_montgomery(pt_out, pt_in, p, a, k, nwords);
	bignum_le_words_to_be(out_xy[0], pt_out[0], (int)nwords);
	bignum_le_words_to_be(out_xy[1], pt_out[1], (int)nwords);
	return 0;
}

/* 0x80f228 */
int ec_point_mul_160(uint8_t **out_xy, const uint8_t *scalar, uint8_t **in_xy,
	       const uint8_t * const *curve)
{
	return ec_point_mul(out_xy, scalar, in_xy, curve, 5u, 0x14u);
}

/* 0x80f23e */
int ec_point_mul_224(uint8_t **out_xy, const uint8_t *scalar, uint8_t **in_xy,
	       const uint8_t * const *curve)
{
	return ec_point_mul(out_xy, scalar, in_xy, curve, 7u, 0x1cu);
}

/* 0x80f254 */
uint32_t bignum_exec_op_a(uint32_t op, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t *status_out)
{
	uint32_t st;

	if ((op & 0xffu) >= 0x1du)
		return 0x800f0116u;
	MMIO_WRITE32(0xe0040800u,
		     (op << 27) | ((w1 & 0xffu) << 18) | ((w2 & 0x1ffu) << 9) | (w3 & 0x1ffu));
	st = bignum_wait();
	if (status_out != NULL)
		*status_out = st;
	return 0;
}

/* 0x80f2b4 */
int ecdsa_sign_core(uint8_t **sig_rs, const uint8_t *hash, const uint8_t *nonce, const uint8_t *priv,
	       const uint8_t * const *curve, uint32_t nwords, uint32_t nbytes)
{
	uint32_t p[7], a[7], n[7], gx[7], gy[7], gz[7];
	uint32_t d[7], k[7], e[7], r[7], s[7], ox[7], oy[7], oz[7];
	uint32_t *in_xy[3] = { gx, gy, gz };
	uint32_t *out_xy[3] = { ox, oy, oz };
	uint32_t p_words = nwords, n_words = nwords, e_words = nwords;
	uint32_t st = 0, i, r_words;

	bignum_be_to_le_words(p, curve[0], (int)nbytes);
	bignum_be_to_le_words(a, curve[1], (int)nbytes);
	bignum_be_to_le_words(n, curve[3], (int)nbytes);
	bignum_be_to_le_words(gx, curve[4], (int)nbytes);
	bignum_be_to_le_words(gy, curve[5], (int)nbytes);
	bignum_be_to_le_words(d, priv, (int)nbytes);
	bignum_be_to_le_words(k, nonce, (int)nbytes);
	bignum_be_to_le_words(e, hash, (int)nbytes);

	while (p_words > 0 && p[p_words - 1u] == 0)
		p_words--;
	while (n_words > 0 && n[n_words - 1u] == 0)
		n_words--;
	while (e_words > 0 && e[e_words - 1u] == 0)
		e_words--;

	if (!bignum_is_valid_scalar(d, n, (int)nwords) || !bignum_is_valid_scalar(k, n, (int)nwords))
		return -1;

	if ((int)n_words < (int)e_words) {
		for (i = 0; i < e_words; i++)
			MMIO_WRITE32(0xe0040000u + i * 4u, e[i]);
		for (i = 0; i < n_words; i++)
			MMIO_WRITE32(0xe0040400u + i * 4u, n[i]);
		bignum_exec_op_a(0x0bu, e_words, n_words, 0u, &st);
		for (i = 0; i < n_words; i++)
			e[i] = MMIO_READ32(0xe0040000u + i * 4u);
	}

	ec_scalar_mul_montgomery(out_xy, in_xy, p, a, k, nwords);
	for (i = 0; i < n_words; i++) {
		MMIO_WRITE32(0xe0040108u + i * 4u, n[i]);
		MMIO_WRITE32(0xe0040508u + i * 4u, k[i]);
	}
	bignum_exec_op_a(0x1cu, 1u, n_words + 0x142u, 0u, &st);
	bignum_exec_op_a(0x16u, n_words, 0u, 0u, &st);
	bignum_exec_op_a(0x17u, n_words, 0u, 0u, &st);
	for (i = 0; i < n_words; i++)
		MMIO_WRITE32(0xe0040400u + i * 4u, n[i]);
	bignum_exec_op_a(0x18u, n_words, 0u, 0x49u, &st);

	for (i = 0; i < p_words; i++)
		MMIO_WRITE32(0xe0040000u + i * 4u, ox[i]);
	bignum_exec_op_a(0x0bu, p_words, n_words, 0u, &st);
	r_words = (st >> 8) & 0xffu;
	bignum_exec_op_a(0x1cu, nwords - r_words, r_words, 0u, &st);

	int r_nonzero = 0;
	for (i = 0; i < r_words; i++) {
		if (MMIO_READ32(0xe0040000u + i * 4u) != 0)
			r_nonzero = 1;
	}
	if (!r_nonzero)
		return -1;
	for (i = 0; i < nwords; i++)
		r[i] = MMIO_READ32(0xe0040000u + i * 4u);

	bignum_exec_op_a(0x18u, n_words, 0u, 0x42u, &st);
	for (i = 0; i < n_words; i++)
		MMIO_WRITE32(0xe0040508u + i * 4u, d[i]);
	bignum_exec_op_a(0x0cu, n_words, 0u, 0u, &st);
	bignum_exec_op_a(0x18u, n_words, 0u, 0x42u, &st);
	for (i = 0; i < n_words; i++)
		MMIO_WRITE32(0xe0040508u + i * 4u, e[i]);
	bignum_exec_op_a(0x0du, n_words, 0u, 0u, &st);
	for (i = 0; i < n_words; i++)
		MMIO_WRITE32(0xe0040508u + i * 4u, MMIO_READ32(0xe0040124u + i * 4u));
	bignum_exec_op_a(0x0fu, 0u, 0u, 0u, &st);
	bignum_exec_op_a(0x10u, n_words, 0u, 0u, &st);
	MMIO_WRITE32(0xe0040108u, 1u);
	bignum_exec_op_a(0x1cu, n_words - 1u, 0x43u, 0u, &st);
	bignum_exec_op_a(0x10u, n_words, 0u, 0u, &st);
	for (i = 0; i < n_words; i++)
		s[i] = MMIO_READ32(0xe0040508u + i * 4u);
	if (bignum_top_word(s, n_words) < 0)
		return -1;
	for (i = n_words; i < nwords; i++)
		s[i] = 0;

	bignum_le_words_to_be(sig_rs[0], r, (int)nwords);
	bignum_le_words_to_be(sig_rs[1], s, (int)nwords);
	return 0;
}

/* 0x80f8f2 */
int ecdsa_sign_160(uint8_t **sig_rs, const uint8_t *hash, const uint8_t *nonce, const uint8_t *priv,
	       const uint8_t * const *curve)
{
	return ecdsa_sign_core(sig_rs, hash, nonce, priv, curve, 5u, 0x14u);
}

/* 0x80f90c */
int ecdsa_sign_224(uint8_t **sig_rs, const uint8_t *hash, const uint8_t *nonce, const uint8_t *priv,
	       const uint8_t * const *curve)
{
	return ecdsa_sign_core(sig_rs, hash, nonce, priv, curve, 7u, 0x1cu);
}

/* 0x80f926 */
uint32_t bignum_exec_op_b(uint32_t op, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t *status_out)
{
	return bignum_exec_op_a(op, w1, w2, w3, status_out);
}

/* 0x80f986 */
int ecdsa_verify_core(uint8_t **pub_xy, const uint8_t *hash, uint8_t **sig_rs,
	       const uint8_t * const *curve, uint32_t nwords, uint32_t nbytes)
{
	uint32_t p[7], a[7], n[7], gx[7], gy[7], gz[7];
	uint32_t qx[7], qy[7], qz[7], r[7], s[7], e[7], u1[7], u2[7];
	uint32_t *p1_xy[3] = { gx, gy, gz };
	uint32_t *p2_xy[3] = { qx, qy, qz };
	uint32_t p_words = nwords, n_words = nwords, e_words = nwords;
	uint32_t st = 0, i, cmd;

	bignum_be_to_le_words(p, curve[0], (int)nbytes);
	bignum_be_to_le_words(a, curve[1], (int)nbytes);
	bignum_be_to_le_words(n, curve[3], (int)nbytes);
	bignum_be_to_le_words(gx, curve[4], (int)nbytes);
	bignum_be_to_le_words(gy, curve[5], (int)nbytes);
	bignum_be_to_le_words(e, hash, (int)nbytes);
	bignum_be_to_le_words(qx, pub_xy[0], (int)nbytes);
	bignum_be_to_le_words(qy, pub_xy[1], (int)nbytes);

	while (p_words > 0 && p[p_words - 1u] == 0)
		p_words--;
	while (n_words > 0 && n[n_words - 1u] == 0)
		n_words--;
	while (e_words > 0 && e[e_words - 1u] == 0)
		e_words--;

	if (bignum_top_word(p, p_words) < 0)
		return -1;

	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040000u + i * 4u, qx[i]);
	for (i = 0; i < p_words; i++)
		MMIO_WRITE32(0xe0040400u + i * 4u, p[i]);
	cmd = ((nwords & 0xffu) << 18) | 0x58000000u | ((p_words & 0x1ffu) << 9);
	bignum_exec(cmd);
	for (i = 0; i < p_words; i++)
		qx[i] = MMIO_READ32(0xe0040000u + i * 4u);

	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040000u + i * 4u, qy[i]);
	bignum_exec(cmd);
	st = MMIO_READ32(0xe0040804u);
	for (i = 0; i < p_words; i++)
		qy[i] = MMIO_READ32(0xe0040000u + i * 4u);

	if ((int)n_words < (int)e_words) {
		for (i = 0; i < e_words; i++)
			MMIO_WRITE32(0xe0040000u + i * 4u, e[i]);
		for (i = 0; i < n_words; i++)
			MMIO_WRITE32(0xe0040400u + i * 4u, n[i]);
		bignum_exec(((e_words & 0xffu) << 18) | ((n_words & 0x1ffu) << 9) | 0x58000000u);
		st = MMIO_READ32(0xe0040804u);
		for (i = 0; i < n_words; i++)
			e[i] = MMIO_READ32(0xe0040000u + i * 4u);
	}

	bignum_be_to_le_words(r, sig_rs[0], (int)nbytes);
	bignum_be_to_le_words(s, sig_rs[1], (int)nbytes);
	if (!bignum_is_valid_scalar(r, n, (int)nwords) || !bignum_is_valid_scalar(s, n, (int)nwords))
		return -1;

	for (i = 0; i < n_words; i++) {
		MMIO_WRITE32(0xe0040108u + i * 4u, n[i]);
		MMIO_WRITE32(0xe0040508u + i * 4u, s[i]);
	}
	bignum_exec_op_b(0x1cu, 1u, n_words + 0x142u, 0u, &st);
	bignum_exec_op_b(0x16u, n_words, 0u, 0u, &st);
	bignum_exec_op_b(0x17u, n_words, 0u, 0u, &st);
	for (i = 0; i < n_words; i++)
		MMIO_WRITE32(0xe0040400u + i * 4u, n[i]);
	bignum_exec_op_b(0x18u, n_words, 0u, 0x42u, &st);
	MMIO_WRITE32(0xe0040508u, 1u);
	bignum_exec_op_b(0x1cu, n_words - 1u, 0x143u, 0u, &st);
	bignum_exec_op_b(0x0fu, 0u, 0u, 0u, &st);
	bignum_exec_op_b(0x10u, n_words, 0u, 0u, &st);
	bignum_exec_op_b(0x18u, n_words, 0x142u, 0x149u, &st);

	for (i = 0; i < n_words; i++)
		MMIO_WRITE32(0xe0040108u + i * 4u, e[i]);
	bignum_exec_op_b(0x10u, n_words, 0u, 0u, &st);
	for (i = 0; i < n_words; i++)
		u1[i] = MMIO_READ32(0xe0040508u + i * 4u);

	bignum_exec_op_b(0x18u, n_words, 0x149u, 0x142u, &st);
	for (i = 0; i < n_words; i++)
		MMIO_WRITE32(0xe0040108u + i * 4u, r[i]);
	bignum_exec_op_b(0x10u, n_words, 0u, 0u, &st);
	for (i = 0; i < n_words; i++)
		u2[i] = MMIO_READ32(0xe0040508u + i * 4u);

	for (i = n_words; i < nwords; i++) {
		u1[i] = 0;
		u2[i] = 0;
	}

	ec_shamir_dual_scalar_mul(p2_xy, p1_xy, p2_xy, u1, u2, p, a, nwords);
	for (i = 0; i < p_words; i++)
		MMIO_WRITE32(0xe0040000u + i * 4u, qx[i]);
	for (i = 0; i < n_words; i++)
		MMIO_WRITE32(0xe0040400u + i * 4u, n[i]);
	bignum_exec_op_b(0x0bu, p_words, n_words, 0u, &st);
	for (i = 0; i < n_words; i++)
		MMIO_WRITE32(0xe0040400u + i * 4u, r[i]);
	bignum_exec_op_b(0x06u, (st >> 8) & 0xffu, n_words, 0u, &st);
	return (int)(((st & 0x20000000u) >> 29) - 1u);
}

/* 0x81017c */
int ecdsa_verify_160(uint8_t **pub_xy, const uint8_t *hash, uint8_t **sig_rs,
	       const uint8_t * const *curve)
{
	return ecdsa_verify_core(pub_xy, hash, sig_rs, curve, 5u, 0x14u);
}

/* 0x810194 */
int ecdsa_verify_224(uint8_t **pub_xy, const uint8_t *hash, uint8_t **sig_rs,
	       const uint8_t * const *curve)
{
	return ecdsa_verify_core(pub_xy, hash, sig_rs, curve, 7u, 0x1cu);
}

/* 0x8101ac */
int bignum_mod_reduce_scalar(uint8_t *out, const uint8_t *in, uint32_t in_words, uint32_t in_bytes,
	       const uint8_t *mod, uint32_t mod_words, uint32_t mod_bytes)
{
	uint32_t in_le[15], rem_le[7], mod_le[9];
	uint32_t m_words = mod_words, i;

	bignum_be_to_le_words(mod_le, mod, (int)mod_bytes);
	bignum_be_to_le_words(in_le, in, (int)in_bytes);
	while (m_words > 0 && mod_le[m_words - 1u] == 0)
		m_words--;

	for (i = 0; i < in_words; i++)
		MMIO_WRITE32(0xe0040000u + i * 4u, in_le[i]);
	for (i = 0; i < m_words; i++)
		MMIO_WRITE32(0xe0040400u + i * 4u, mod_le[i]);
	bignum_exec(((in_words & 0xffu) << 18) | 0x58000000u | ((m_words & 0x1ffu) << 9));
	for (i = 0; i < m_words; i++)
		rem_le[i] = MMIO_READ32(0xe0040000u + i * 4u);

	if (bignum_top_word(rem_le, m_words) >= 0) {
		for (i = m_words; i < mod_words; i++)
			rem_le[i] = 0;
		bignum_le_words_to_be(out, rem_le, (int)mod_words);
		return 0;
	}
	return -1;
}

/* 0x810372 */
int bignum_mod_reduce_160(uint8_t *out, const uint8_t *in, const uint8_t *mod)
{
	return bignum_mod_reduce_scalar(out, in, 10u, 0x28u, mod, 5u, 0x14u);
}

/* 0x81038e */
int bignum_mod_reduce_224(uint8_t *out, const uint8_t *in, const uint8_t *mod)
{
	return bignum_mod_reduce_scalar(out, in, 15u, 0x3cu, mod, 7u, 0x1cu);
}

/* 0x8103aa */
int bignum_cmp_gt(const uint32_t *a, const uint32_t *b, int nwords)
{
	int i = nwords;

	if (nwords < 1)
		return 0;
	while (i > 0) {
		i--;
		if (a[i] != b[i])
			return b[i] < a[i];
	}
	return 0;
}

/* 0x8103d6 */
void bignum_shl_words(uint32_t *dst, const uint32_t *src, int nwords, uint32_t shift)
{
	int i;

	if ((shift & 0x3fu) == 0) {
		for (i = 0; i < nwords; i++)
			dst[i] = src[i];
	} else if (nwords > 0) {
		uint32_t carry = 0;
		uint32_t sh = shift & 0x1fu;
		uint32_t rsh = (32u - (shift & 0x3fu)) & 0x1fu;
		for (i = 0; i < nwords; i++) {
			uint32_t v = src[i];
			dst[i] = (v << sh) | carry;
			carry = v >> rsh;
		}
	}
}

/* 0x810414 */
void bignum_be_to_le_words(uint32_t *dst_le, const uint8_t *src_be, int nbytes)
{
	int nwords = nbytes >> 2;
	int i;

	for (i = 0; i < nwords; i++) {
		const uint8_t *p = src_be + nbytes - 1 - i * 4;
		dst_le[i] = (uint32_t)p[0] |
			    ((uint32_t)p[-1] << 8) |
			    ((uint32_t)p[-2] << 16) |
			    ((uint32_t)p[-3] << 24);
	}
}

/* 0x810458 */
void bignum_le_words_to_be(uint8_t *dst_be, const uint32_t *src_le, int nwords)
{
	int i;

	for (i = 0; i < nwords; i++) {
		uint32_t v = src_le[nwords - 1 - i];
		dst_be[i * 4 + 0] = (uint8_t)(v >> 24);
		dst_be[i * 4 + 1] = (uint8_t)(v >> 16);
		dst_be[i * 4 + 2] = (uint8_t)(v >> 8);
		dst_be[i * 4 + 3] = (uint8_t)v;
	}
}

/* 0x81048a */
void ec_jacobian_to_affine_step(volatile uint32_t *pt_regs, uint32_t reg_idx, uint32_t nwords)
{
	uint32_t i, wcmd, mcmd, sqcmd, subcmd, addcmd, modcmd, c_10e;
	uintptr_t addr = (uintptr_t)pt_regs;

	if (addr < 0xe0040454u || addr >= 0xe0040610u || reg_idx >= 0x84u || nwords - 1u >= 7u)
		return;

	int z1_nonzero = 0;
	for (i = 0; i < nwords; i++) {
		if (MMIO_READ32(0xe00401e8u + i * 4u) != 0)
			z1_nonzero = 1;
	}
	if (!z1_nonzero) {
		for (i = 0; i < nwords; i++) {
			MMIO_WRITE32(0xe00401b0u + i * 4u, pt_regs[i]);
			MMIO_WRITE32(0xe00401ccu + i * 4u, pt_regs[7u + i]);
			MMIO_WRITE32(0xe00401e8u + i * 4u, pt_regs[14u + i]);
		}
		return;
	}

	int z2_nonzero = 0;
	for (i = 0; i < nwords; i++) {
		if (pt_regs[14u + i] != 0)
			z2_nonzero = 1;
	}
	if (!z2_nonzero)
		return;

	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040508u + i * 4u, MMIO_READ32(0xe00401e8u + i * 4u));
	wcmd = (nwords & 0xffu) << 18;
	sqcmd = wcmd | 0x88000000u;
	mcmd = wcmd | 0x80000000u;
	modcmd = wcmd | 0xc0009242u;
	subcmd = wcmd | 0x70000000u;
	addcmd = wcmd | 0xc002a142u;
	c_10e = (((reg_idx + 0x10eu) << 9) & 0x0003fe00u) | 0x142u | wcmd | 0xc0000000u;

	bignum_exec(sqcmd);
	bignum_exec(wcmd | 0xc0028549u);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040108u + i * 4u, pt_regs[i]);
	bignum_exec(mcmd);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040124u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
	bignum_exec(c_10e);
	bignum_exec(sqcmd);
	bignum_exec(wcmd | 0xc0028550u);
	bignum_exec(wcmd | 0xc000d842u);
	bignum_exec(mcmd);
	bignum_exec(modcmd);
	bignum_exec(subcmd);
	bignum_exec(wcmd | 0xc0008450u);
	bignum_exec(modcmd);
	bignum_exec(wcmd | 0x68000000u);
	bignum_exec(wcmd | 0xc0008449u);
	bignum_exec(wcmd | 0xc000f442u);
	bignum_exec(wcmd | 0xc0029342u);
	bignum_exec(mcmd);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040108u + i * 4u, pt_regs[7u + i]);
	bignum_exec(mcmd);
	for (i = 0; i < nwords; i++) {
		MMIO_WRITE32(0xe0040178u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
		MMIO_WRITE32(0xe0040108u + i * 4u, pt_regs[14u + i]);
	}
	bignum_exec(addcmd);
	bignum_exec(mcmd);
	bignum_exec(wcmd | 0xc000e642u);
	bignum_exec(mcmd);
	bignum_exec(wcmd | 0xc000bc42u);
	bignum_exec(subcmd);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040540u + i * 4u, MMIO_READ32(0xe0040108u + i * 4u));
	bignum_exec(wcmd | 0xc000bc42u);
	bignum_exec(wcmd | 0x68000000u);
	bignum_exec(wcmd | 0xc000845eu);

	int dx_nonzero = 0;
	for (i = 0; i < nwords; i++) {
		if (MMIO_READ32(0xe0040140u + i * 4u) != 0)
			dx_nonzero = 1;
	}
	if (!dx_nonzero) {
		int dy_nonzero = 0;
		for (i = 0; i < nwords; i++) {
			if (MMIO_READ32(0xe0040540u + i * 4u) != 0)
				dy_nonzero = 1;
		}
		if (!dy_nonzero) {
			ec_point_add_jacobian(nwords);
			return;
		}
		bignum_exec(0xe054d800u);
		MMIO_WRITE32(0xe00401b0u, 1u);
		MMIO_WRITE32(0xe00401ccu, 1u);
		return;
	}

	bignum_exec(c_10e);
	bignum_exec(wcmd | 0xc000f442u);
	bignum_exec(mcmd);
	bignum_exec(wcmd | 0xc000a042u);
	bignum_exec(mcmd);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe00401e8u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
	bignum_exec(addcmd);
	bignum_exec(sqcmd);
	for (i = 0; i < nwords; i++) {
		MMIO_WRITE32(0xe004015cu + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
		MMIO_WRITE32(0xe0040508u + i * 4u, MMIO_READ32(0xe0040140u + i * 4u));
	}
	bignum_exec(sqcmd);
	bignum_exec(wcmd | 0xc0028549u);
	bignum_exec(modcmd);
	bignum_exec(mcmd);
	bignum_exec(wcmd | 0xc000ae42u);
	bignum_exec(subcmd);
	bignum_exec(wcmd | 0xc000846cu);
	for (i = 0; i < nwords; i++) {
		MMIO_WRITE32(0xe0040108u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
		MMIO_WRITE32(0xe0040508u + i * 4u, MMIO_READ32(0xe00401b0u + i * 4u));
	}
	bignum_exec(subcmd);
	bignum_exec(subcmd);
	bignum_exec(addcmd);
	bignum_exec(mcmd);
	bignum_exec(wcmd | 0xc0028550u);
	bignum_exec(wcmd | 0xc000a042u);
	bignum_exec(wcmd | 0xc0029342u);
	bignum_exec(mcmd);
	bignum_exec(wcmd | 0xc000bc42u);
	bignum_exec(mcmd);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040108u + i * 4u, MMIO_READ32(0xe0040540u + i * 4u));
	bignum_exec(subcmd);
	bignum_exec(wcmd | 0xc0008400u);

	uint32_t hi_bit = 0;
	if ((MMIO_READ32(0xe0040000u) & 1u) != 0) {
		MMIO_WRITE32(0xe0040800u, ((nwords & 0x1ffu) << 9) | wcmd | 0x08000000u);
		uint32_t st = bignum_wait();
		hi_bit = ((st & 0x40000000u) >> 30) << 31;
	}
	bignum_exec(wcmd | 0xc8000200u);
	bignum_exec(wcmd | 0xc0000073u);
	MMIO_WRITE32(0xe00401ccu + (nwords - 1u) * 4u,
		     MMIO_READ32(0xe00401ccu + (nwords - 1u) * 4u) | hi_bit);
}

/* 0x8110c0 */
void ec_point_add_jacobian(uint32_t nwords)
{
	uint32_t i, wcmd, sqcmd, addcmd, mcmd, subcmd, modcmd, c_293;

	if (nwords - 1u >= 7u)
		return;
	int z_nonzero = 0;
	for (i = 0; i < nwords; i++) {
		if (MMIO_READ32(0xe00401e8u + i * 4u) != 0)
			z_nonzero = 1;
	}
	if (!z_nonzero)
		return;

	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040508u + i * 4u, MMIO_READ32(0xe00401b0u + i * 4u));
	wcmd = (nwords & 0xffu) << 18;
	sqcmd = wcmd | 0x88000000u;
	addcmd = wcmd | 0x68000000u;
	mcmd = wcmd | 0x80000000u;
	subcmd = wcmd | 0x70000000u;
	modcmd = ((nwords & 0x1ffu) << 9) | 0x58000000u | (((nwords + 1u) << 18) & 0x03fc0000u);
	c_293 = wcmd | 0xc0029342u;

	bignum_exec(sqcmd);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040108u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
	bignum_exec(addcmd);
	bignum_exec(addcmd);
	bignum_exec(wcmd | 0xc0008449u);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040508u + i * 4u, MMIO_READ32(0xe00401e8u + i * 4u));
	bignum_exec(wcmd | 0xc0028549u);
	bignum_exec(sqcmd);
	bignum_exec(sqcmd);
	bignum_exec(wcmd | 0xc000ca42u);
	bignum_exec(mcmd);
	bignum_exec(wcmd | 0xc0009242u);
	bignum_exec(addcmd);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040508u + i * 4u, MMIO_READ32(0xe0040108u + i * 4u));
	bignum_exec(wcmd | 0xc0028550u);
	bignum_exec(sqcmd);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040124u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
	bignum_exec(wcmd | 0xc000e642u);
	bignum_exec(c_293);
	bignum_exec(mcmd);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040108u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
	bignum_exec(addcmd);
	bignum_exec(wcmd | 0xc000847au);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040508u + i * 4u, MMIO_READ32(0xe00401ccu + i * 4u));
	bignum_exec(sqcmd);
	bignum_exec(wcmd | 0xc0028549u);
	bignum_exec(wcmd | 0xc000d842u);
	bignum_exec(mcmd);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040000u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
	bignum_exec(wcmd | 0xd8000400u);
	bignum_exec(modcmd);
	bignum_exec(wcmd | 0xc0000050u);
	bignum_exec(wcmd | 0xd8000200u);
	bignum_exec(modcmd);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040508u + i * 4u, MMIO_READ32(0xe0040000u + i * 4u));
	bignum_exec(0x78000000u);
	bignum_exec(wcmd | 0xc0009242u);
	bignum_exec(subcmd);
	bignum_exec(wcmd | 0xc000846cu);
	bignum_exec(c_293);
	bignum_exec(sqcmd);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040000u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
	bignum_exec(wcmd | 0xd8000600u);
	bignum_exec(modcmd);
	bignum_exec(0x78000000u);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040524u + i * 4u, MMIO_READ32(0xe0040000u + i * 4u));
	bignum_exec(wcmd | 0xc000a042u);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040508u + i * 4u, MMIO_READ32(0xe00401b0u + i * 4u));
	bignum_exec(subcmd);
	bignum_exec(wcmd | 0xc002a142u);
	bignum_exec(mcmd);
	for (i = 0; i < nwords; i++)
		MMIO_WRITE32(0xe0040108u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));
	bignum_exec(c_293);
	bignum_exec(subcmd);
	bignum_exec(wcmd | 0xc0008473u);
}

/* 0x811944 */
void ec_shamir_dual_scalar_mul(uint32_t **out_xy, uint32_t **p1_xy, uint32_t **p2_xy,
		const uint32_t *s1, const uint32_t *s2,
		const uint32_t *p, const uint32_t *a, uint32_t nwords)
{
	uint32_t p_words = nwords, i, wcmd, mcmd, c0_cmd;
	int p1_nonzero = 0, p2_nonzero = 0;

	if (nwords - 1u > 6u)
		return;
	while (p_words > 0 && p[p_words - 1u] == 0)
		p_words--;
	if (p_words == 0)
		return;

	MMIO_WRITE32(0xe0040000u + p_words * 8u, 1u);
	bignum_exec(((p_words & 0x7fu) << 19) | 0xe0000000u);
	for (i = 0; i < p_words; i++)
		MMIO_WRITE32(0xe0040400u + i * 4u, p[i]);
	bignum_exec(((p_words & 0x1ffu) << 9) | 0x58000000u | (((p_words * 2u + 1u) << 18) & 0x03fc0000u));
	wcmd = (p_words & 0xffu) << 18;
	c0_cmd = wcmd | 0xc0000000u;
	mcmd = wcmd | 0x80000000u;
	bignum_exec(wcmd | 0xc0000042u);
	bignum_exec(0x78000000u);

	for (i = 0; i < p_words; i++) {
		if (p1_xy[0][i] != 0 || p1_xy[1][i] != 0)
			p1_nonzero = 1;
	}
	if (!p1_nonzero) {
		bignum_exec(0xe0562a00u);
		MMIO_WRITE32(0xe0040454u, 1u);
		MMIO_WRITE32(0xe0040470u, 1u);
	} else {
		for (i = 0; i < p_words; i++)
			MMIO_WRITE32(0xe0040508u + i * 4u, p1_xy[0][i]);
		bignum_exec(mcmd);
		bignum_exec(wcmd | 0xc0028515u);
		for (i = 0; i < p_words; i++)
			MMIO_WRITE32(0xe0040508u + i * 4u, p1_xy[1][i]);
		bignum_exec(mcmd);
		bignum_exec(wcmd | 0xc002851cu);
		MMIO_WRITE32(0xe0040508u, 1u);
		bignum_exec((((p_words - 1u) << 18) & 0x03fc0000u) | 0xe0028600u);
		bignum_exec(mcmd);
		bignum_exec(wcmd | 0xc0028523u);
	}

	for (i = 0; i < p_words; i++) {
		if (p2_xy[0][i] != 0 || p2_xy[1][i] != 0)
			p2_nonzero = 1;
	}
	if (!p2_nonzero) {
		bignum_exec(0xe0565400u);
		MMIO_WRITE32(0xe00404a8u, 1u);
		MMIO_WRITE32(0xe00404c4u, 1u);
	} else {
		for (i = 0; i < p_words; i++)
			MMIO_WRITE32(0xe0040508u + i * 4u, p2_xy[0][i]);
		bignum_exec(mcmd);
		bignum_exec(wcmd | 0xc002852au);
		for (i = 0; i < p_words; i++)
			MMIO_WRITE32(0xe0040508u + i * 4u, p2_xy[1][i]);
		bignum_exec(mcmd);
		bignum_exec(wcmd | 0xc0028531u);
		bignum_exec(wcmd | 0xc0024738u);
	}

	for (i = 0; i < p_words; i++)
		MMIO_WRITE32(0xe0040508u + i * 4u, a[i]);
	bignum_exec(mcmd);
	for (i = 0; i < p_words; i++)
		MMIO_WRITE32(0xe0040194u + i * 4u, MMIO_READ32(0xe0040508u + i * 4u));

	uint32_t clz1 = 0, clz2 = 0;
	for (i = 0; i < nwords; i++) {
		uint32_t c = count_leading_zeros_u32(s1[nwords - 1u - i]);
		clz1 += c;
		if (c != 32u)
			break;
	}
	for (i = 0; i < nwords; i++) {
		uint32_t c = count_leading_zeros_u32(s2[nwords - 1u - i]);
		clz2 += c;
		if (c != 32u)
			break;
	}
	uint32_t min_clz = (clz1 < clz2) ? clz1 : clz2;
	int bit_pos = (int)(nwords * 32u - min_clz);

	for (i = 0; i < p_words; i++) {
		MMIO_WRITE32(0xe00401b0u + i * 4u, MMIO_READ32(0xe0040454u + i * 4u));
		MMIO_WRITE32(0xe00401ccu + i * 4u, MMIO_READ32(0xe0040470u + i * 4u));
		MMIO_WRITE32(0xe00401e8u + i * 4u, MMIO_READ32(0xe004048cu + i * 4u));
	}
	ec_jacobian_to_affine_step(MMIO_PTR(0xe00404a8u), 0x2au, p_words);
	for (i = 0; i < p_words; i++) {
		MMIO_WRITE32(0xe004055cu + i * 4u, MMIO_READ32(0xe00401b0u + i * 4u));
		MMIO_WRITE32(0xe0040578u + i * 4u, MMIO_READ32(0xe00401ccu + i * 4u));
		MMIO_WRITE32(0xe0040594u + i * 4u, MMIO_READ32(0xe00401e8u + i * 4u));
	}
	bignum_exec(0xe054d800u);
	MMIO_WRITE32(0xe00401b0u, 1u);
	MMIO_WRITE32(0xe00401ccu, 1u);

	while (--bit_pos >= 0) {
		ec_point_add_jacobian(p_words);
		uint32_t widx = (uint32_t)bit_pos >> 5;
		uint32_t bshift = (uint32_t)bit_pos & 0x1fu;
		uint32_t pair = (((s1[widx] >> bshift) & 1u) << 1) | ((s2[widx] >> bshift) & 1u);
		if (pair == 2u)
			ec_jacobian_to_affine_step(MMIO_PTR(0xe0040454u), 0x15u, p_words);
		else if (pair == 1u)
			ec_jacobian_to_affine_step(MMIO_PTR(0xe00404a8u), 0x2au, p_words);
		else if (pair == 3u)
			ec_jacobian_to_affine_step(MMIO_PTR(0xe004055cu), 0x57u, p_words);
	}

	int z_nonzero = 0;
	for (i = 0; i < p_words; i++) {
		if (MMIO_READ32(0xe00401e8u + i * 4u) != 0)
			z_nonzero = 1;
	}
	if (!z_nonzero) {
		for (i = 0; i < p_words; i++) {
			out_xy[0][i] = 0;
			out_xy[1][i] = 0;
		}
		return;
	}

	bignum_exec(c0_cmd | 0xd82au);
	bignum_exec(c0_cmd | 0xe631u);
	for (i = 0; i < p_words; i++) {
		MMIO_WRITE32(0xe0040108u + i * 4u, p[i]);
		MMIO_WRITE32(0xe0040508u + i * 4u, MMIO_READ32(0xe00401e8u + i * 4u));
	}
	bignum_exec((((p_words + 0x142u) << 9) & 0x0003fe00u) | 0xe0040000u);
	bignum_exec(wcmd | 0xb0000000u);
	bignum_exec(wcmd | 0xb8000000u);
	for (i = 0; i < p_words; i++)
		MMIO_WRITE32(0xe0040400u + i * 4u, p[i]);
	bignum_exec(c0_cmd | 0x546cu);
	bignum_exec(c0_cmd | 0x6273u);
	bignum_exec(wcmd | 0xc0000042u);
	MMIO_WRITE32(0xe0040508u, 1u);
	bignum_exec((((p_words - 1u) & 0xffu) << 18) | 0xe0028600u);
	bignum_exec(mcmd);
	bignum_exec(mcmd);
	bignum_exec(c0_cmd | 0x28549u);
	bignum_exec(mcmd);
	bignum_exec(c0_cmd | 0xe642u);
	bignum_exec(mcmd);
	for (i = 0; i < p_words; i++)
		out_xy[1][i] = MMIO_READ32(0xe0040508u + i * 4u);
	bignum_exec(c0_cmd | 0xd842u);
	bignum_exec(c0_cmd | 0x29342u);
	bignum_exec(mcmd);
	for (i = 0; i < p_words; i++)
		out_xy[0][i] = MMIO_READ32(0xe0040508u + i * 4u);
	for (i = p_words; i < nwords; i++) {
		out_xy[0][i] = 0;
		out_xy[1][i] = 0;
	}
}

/* 0x81257c */
void sm_halt_loop(void)
{
	for (;;)
		;
}

/* 0x81257e */
void *sm_memset(void *dst, int val, uint32_t len)
{
	uint8_t *p = (uint8_t *)dst;
	uint8_t b = (uint8_t)val;
	uint32_t i;

	for (i = 0; i < len; i++)
		p[i] = b;
	return dst;
}

/* 0x8125d4 */
void *sm_memcpy(void *dst, const void *src, uint32_t len)
{
	uintptr_t d = (uintptr_t)dst;
	uintptr_t s = (uintptr_t)src;
	uint32_t i;

	if (((d | s | len) & 3u) == 0) {
		uint32_t *dw = (uint32_t *)dst;
		const uint32_t *sw = (const uint32_t *)src;
		for (i = 0; i < (len >> 2); i++)
			dw[i] = sw[i];
	} else {
		uint8_t *db = (uint8_t *)dst;
		const uint8_t *sb = (const uint8_t *)src;
		for (i = 0; i < len; i++)
			db[i] = sb[i];
	}
	return dst;
}

/* 0x812610 */
uint8_t sm_memcmp_consttime(const void *a, const void *b, int len)
{
	const uint8_t *pa = (const uint8_t *)a;
	const uint8_t *pb = (const uint8_t *)b;
	uint8_t diff = 0;
	int i;

	for (i = 0; i < len; i++)
		diff |= (uint8_t)(pa[i] ^ pb[i]);
	return diff;
}

/* 0x81262a */
int32_t sm_mulsi3(uint32_t a, int32_t b)
{
	int32_t res = 0;

	while (a != 0) {
		if ((a & 1u) != 0)
			res += b;
		a >>= 1;
		b <<= 1;
	}
	return res;
}

/* 0x812644 */
uint32_t sm_udivmodsi4(uint32_t num, uint32_t den, int want_rem)
{
	uint32_t bit = 1u;
	uint32_t quot = 0u;
	int iter = 32;

	if (den < num && (int32_t)den >= 0) {
		do {
			den <<= 1;
			bit <<= 1;
			if (num <= den) {
				if (bit == 0)
					return want_rem ? num : 0u;
				break;
			}
			if (--iter == 0)
				return want_rem ? num : 0u;
		} while ((int32_t)den >= 0);
	}
	for (;;) {
		if (den <= num) {
			num -= den;
			quot |= bit;
		}
		bit >>= 1;
		if (bit == 0)
			break;
		den >>= 1;
	}
	return want_rem ? num : quot;
}

/* 0x81268a */
int32_t sm_divsi3(int32_t a, int32_t b)
{
	int neg = 0;
	uint32_t ua, ub, q;

	if (a < 0) {
		ua = (uint32_t)(-a);
		neg = 1;
	} else {
		ua = (uint32_t)a;
	}
	if (b < 0) {
		ub = (uint32_t)(-b);
		neg ^= 1;
	} else {
		ub = (uint32_t)b;
	}
	q = sm_udivmodsi4(ua, ub, 0);
	return neg ? -(int32_t)q : (int32_t)q;
}

/* 0x8126bc */
int32_t sm_modsi3(int32_t a, int32_t b)
{
	uint32_t ub = (b < 0) ? (uint32_t)(-b) : (uint32_t)b;

	if (a >= 0)
		return (int32_t)sm_udivmodsi4((uint32_t)a, ub, 1);
	return -(int32_t)sm_udivmodsi4((uint32_t)(-a), ub, 1);
}

/* 0x8126ec */
uint32_t sm_udivsi3(uint32_t a, uint32_t b)
{
	return sm_udivmodsi4(a, b, 0);
}

/* 0x8126f2 */
uint32_t sm_umodsi3(uint32_t a, uint32_t b)
{
	return sm_udivmodsi4(a, b, 1);
}

#ifndef SM_HOST_TEST
void *memset(void *dst, int val, size_t len)
{
	return sm_memset(dst, val, (uint32_t)len);
}

void *memcpy(void *dst, const void *src, size_t len)
{
	return sm_memcpy(dst, src, (uint32_t)len);
}

int32_t __mulsi3(uint32_t a, int32_t b)
{
	return sm_mulsi3(a, b);
}

int32_t __divsi3(int32_t a, int32_t b)
{
	return sm_divsi3(a, b);
}

int32_t __modsi3(int32_t a, int32_t b)
{
	return sm_modsi3(a, b);
}

uint32_t __udivsi3(uint32_t a, uint32_t b)
{
	return sm_udivsi3(a, b);
}

uint32_t __umodsi3(uint32_t a, uint32_t b)
{
	return sm_umodsi3(a, b);
}
#endif
