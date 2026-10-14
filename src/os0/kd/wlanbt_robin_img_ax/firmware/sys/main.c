/*
 * Robin firmware entry: main() (0x0000039c) and tx_application_define()
 * (0x0000041e).
 *
 * __rt_entry calls main(), which installs the ROM ThreadX hooks
 * (_tx_initialize_low_level, tx_application_define, the IRQ hook) and enters
 * the kernel. The kernel calls tx_application_define() once, which runs the
 * firmware init table.
 */
#include "robin/sys.h"
#include "robin/threadx.h"

void *sysFirstUnusedMemory __attribute__((section(".dtcm.rw")));

int main(void)
{
	sysRegisterRomHooks();
	tx_kernel_enter();
	return 0;
}

void tx_application_define(void *first_unused_memory)
{
	sysFirstUnusedMemory = first_unused_memory;
	sysRunInitTable();
}
