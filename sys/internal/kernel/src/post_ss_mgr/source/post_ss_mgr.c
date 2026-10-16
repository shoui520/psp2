/* Complete native functions are in frames.S. Preserve the original Kernel
 * import identity: the SDK's same-NID Driver entry belongs to another library.
 */
#include <psp2/types.h>

__asm__(
	".pushsection .vitalink.fstubs.SceSysrootForKernel,\"ax\",%progbits\n"
	".balign 16\n"
	".arm\n"
	".global SceSysrootForKernel_55392965\n"
	".type SceSysrootForKernel_55392965, %function\n"
	"SceSysrootForKernel_55392965:\n"
	".word 0x10, 0x3691DA45, 0x55392965, 0xE320F000\n"
	".popsection\n"
);
