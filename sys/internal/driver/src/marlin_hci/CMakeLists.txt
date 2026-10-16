# Original module/export/import tables; C sources follow original address order.
psp2_module(os0_kd_marlin_hci KIND kernel SCE_LAYOUT SOURCES
	buffer.c
	memory.c
    storage.c
    key.c
    search.c
    key_load.c
    library.c
    buffer_ops.c
    storage_parse.c
    handles.c
    record_crypto.c
    stored_io.c
    cipher.c
    hash.c
    random_fill.c
    integer_size.c
    mask.c
    integer.c
    rsa.c
    rsa_padding.c
    rsa_context.c
    rsa_operations.c
    hash_context.c
    random.c
    asn1_key.c
    string.c
    path.c
    record_secret.c
    file.c
    directory.c
    asn1.c
    integer_words.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_random.c
    rsa_crt.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_private.c
    rsa_exponent.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_public_operation.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_encrypt.c
    rsa_sign.c
    integer_encode.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_integer_arithmetic.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_montgomery.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_integer_multiply.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_integer_remainder.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_integer_square.c
    ${CMAKE_SOURCE_DIR}/common/crypto/rsa_integer_tail.c
	marlin_hci.c
    kernel_wrappers.c
    exports.c
    protected_exports.c
)

# The original library prefix is ARM without optimization; wrappers are Thumb.
# Source filenames are descriptive because this module has no debug records.
set_source_files_properties(buffer.c memory.c storage.c
    key.c search.c key_load.c library.c
    buffer_ops.c storage_parse.c handles.c
    record_crypto.c stored_io.c cipher.c hash.c
    random_fill.c integer_size.c mask.c integer.c
    rsa.c rsa_padding.c rsa_context.c rsa_operations.c hash_context.c random.c asn1_key.c string.c path.c record_secret.c file.c directory.c asn1.c PROPERTIES
	COMPILE_OPTIONS "-marm;-O0;-fno-stack-protector")
