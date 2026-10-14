# Original module/export/import tables; C sources follow original address order.
psp2_module(os0_kd_marlin_hci KIND kernel SCE_LAYOUT SOURCES
	source/buffer.c
	source/memory.c
    source/storage.c
    source/key.c
    source/search.c
    source/key_load.c
    source/library.c
    source/buffer_ops.c
    source/storage_parse.c
    source/handles.c
    source/record_crypto.c
    source/stored_io.c
    source/cipher.c
    source/hash.c
    source/random_fill.c
    source/integer_size.c
    source/mask.c
    source/integer.c
    source/rsa.c
    source/rsa_padding.c
    source/rsa_context.c
    source/rsa_operations.c
    source/hash_context.c
    source/random.c
    source/asn1_key.c
    source/string.c
    source/path.c
    source/record_secret.c
    source/file.c
    source/directory.c
    source/asn1.c
    source/integer_words.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_random.c
    source/rsa_crt.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_private.c
    source/rsa_exponent.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_public_operation.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_encrypt.c
    source/rsa_sign.c
    source/integer_encode.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_integer_arithmetic.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_montgomery.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_integer_multiply.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_integer_remainder.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_integer_square.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_integer_tail.c
	source/marlin_hci.c
    source/kernel_wrappers.c
    source/exports.c
    source/protected_exports.c
)

# The original library prefix is ARM without optimization; wrappers are Thumb.
# Source filenames are descriptive because this module has no debug records.
set_source_files_properties(source/buffer.c source/memory.c source/storage.c
    source/key.c source/search.c source/key_load.c source/library.c
    source/buffer_ops.c source/storage_parse.c source/handles.c
    source/record_crypto.c source/stored_io.c source/cipher.c source/hash.c
    source/random_fill.c source/integer_size.c source/mask.c source/integer.c
    source/rsa.c source/rsa_padding.c source/rsa_context.c source/rsa_operations.c source/hash_context.c source/random.c source/asn1_key.c source/string.c source/path.c source/record_secret.c source/file.c source/directory.c source/asn1.c PROPERTIES
	COMPILE_OPTIONS "-marm;-O0;-fno-stack-protector")
