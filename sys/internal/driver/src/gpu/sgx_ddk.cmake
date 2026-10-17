# The IMG SGX DDK (pristine in contrib/) whose headers the GPU modules include by name, from
# the DDK's own include directories, as its sources do ("sgxmpdefs.h", "servicesext.h").
set(PSP2_SGX_DDK "${PSP2_SRC}/contrib/img-sgx-ddk-1.7.862890")
set(PSP2_SGX_DDK_INCLUDE_DIRS "${PSP2_SGX_DDK}/include4" "${PSP2_SGX_DDK}/services4/srvkm/hwdefs")
