/* FAT formatting helpers; source-family attribution is inferred. */

#include "fat_format_internal.h"
#include <exfatfs/fat_boot.h>
#include <exfatfs/exfat_directory.h>
#include <sdstor/mbr.h>
#include <sdstor/sdstor.h>
#include <sysmem/debug.h>
#include <psp2kern/io/fcntl.h>
#include <string.h>
#include <stddef.h>

/* Compressed Unicode uppercase mapping. 0xffff followed by a count skips
 * unchanged code points; the final 0xffff maps U+FFFF itself. */
static const uint16_t exfatDefaultUpcaseTable[2918] = {
	0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, /* U+0000 */
	0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f, /* U+0008 */
	0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, /* U+0010 */
	0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f, /* U+0018 */
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, /* U+0020 */
	0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f, /* U+0028 */
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, /* U+0030 */
	0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f, /* U+0038 */
	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, /* U+0040 */
	0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f, /* U+0048 */
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, /* U+0050 */
	0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, /* U+0058 */
	0x0060, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, /* U+0060 */
	0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f, /* U+0068 */
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, /* U+0070 */
	0x0058, 0x0059, 0x005a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f, /* U+0078 */
	0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087, /* U+0080 */
	0x0088, 0x0089, 0x008a, 0x008b, 0x008c, 0x008d, 0x008e, 0x008f, /* U+0088 */
	0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, /* U+0090 */
	0x0098, 0x0099, 0x009a, 0x009b, 0x009c, 0x009d, 0x009e, 0x009f, /* U+0098 */
	0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7, /* U+00A0 */
	0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af, /* U+00A8 */
	0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7, /* U+00B0 */
	0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf, /* U+00B8 */
	0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7, /* U+00C0 */
	0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf, /* U+00C8 */
	0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7, /* U+00D0 */
	0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df, /* U+00D8 */
	0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7, /* U+00E0 */
	0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf, /* U+00E8 */
	0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00f7, /* U+00F0 */
	0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x0178, /* U+00F8 */
	0x0100, 0x0100, 0x0102, 0x0102, 0x0104, 0x0104, 0x0106, 0x0106, /* U+0100 */
	0x0108, 0x0108, 0x010a, 0x010a, 0x010c, 0x010c, 0x010e, 0x010e, /* U+0108 */
	0x0110, 0x0110, 0x0112, 0x0112, 0x0114, 0x0114, 0x0116, 0x0116, /* U+0110 */
	0x0118, 0x0118, 0x011a, 0x011a, 0x011c, 0x011c, 0x011e, 0x011e, /* U+0118 */
	0x0120, 0x0120, 0x0122, 0x0122, 0x0124, 0x0124, 0x0126, 0x0126, /* U+0120 */
	0x0128, 0x0128, 0x012a, 0x012a, 0x012c, 0x012c, 0x012e, 0x012e, /* U+0128 */
	0x0130, 0x0131, 0x0132, 0x0132, 0x0134, 0x0134, 0x0136, 0x0136, /* U+0130 */
	0x0138, 0x0139, 0x0139, 0x013b, 0x013b, 0x013d, 0x013d, 0x013f, /* U+0138 */
	0x013f, 0x0141, 0x0141, 0x0143, 0x0143, 0x0145, 0x0145, 0x0147, /* U+0140 */
	0x0147, 0x0149, 0x014a, 0x014a, 0x014c, 0x014c, 0x014e, 0x014e, /* U+0148 */
	0x0150, 0x0150, 0x0152, 0x0152, 0x0154, 0x0154, 0x0156, 0x0156, /* U+0150 */
	0x0158, 0x0158, 0x015a, 0x015a, 0x015c, 0x015c, 0x015e, 0x015e, /* U+0158 */
	0x0160, 0x0160, 0x0162, 0x0162, 0x0164, 0x0164, 0x0166, 0x0166, /* U+0160 */
	0x0168, 0x0168, 0x016a, 0x016a, 0x016c, 0x016c, 0x016e, 0x016e, /* U+0168 */
	0x0170, 0x0170, 0x0172, 0x0172, 0x0174, 0x0174, 0x0176, 0x0176, /* U+0170 */
	0x0178, 0x0179, 0x0179, 0x017b, 0x017b, 0x017d, 0x017d, 0x017f, /* U+0178 */
	0x0243, 0x0181, 0x0182, 0x0182, 0x0184, 0x0184, 0x0186, 0x0187, /* U+0180 */
	0x0187, 0x0189, 0x018a, 0x018b, 0x018b, 0x018d, 0x018e, 0x018f, /* U+0188 */
	0x0190, 0x0191, 0x0191, 0x0193, 0x0194, 0x01f6, 0x0196, 0x0197, /* U+0190 */
	0x0198, 0x0198, 0x023d, 0x019b, 0x019c, 0x019d, 0x0220, 0x019f, /* U+0198 */
	0x01a0, 0x01a0, 0x01a2, 0x01a2, 0x01a4, 0x01a4, 0x01a6, 0x01a7, /* U+01A0 */
	0x01a7, 0x01a9, 0x01aa, 0x01ab, 0x01ac, 0x01ac, 0x01ae, 0x01af, /* U+01A8 */
	0x01af, 0x01b1, 0x01b2, 0x01b3, 0x01b3, 0x01b5, 0x01b5, 0x01b7, /* U+01B0 */
	0x01b8, 0x01b8, 0x01ba, 0x01bb, 0x01bc, 0x01bc, 0x01be, 0x01f7, /* U+01B8 */
	0x01c0, 0x01c1, 0x01c2, 0x01c3, 0x01c4, 0x01c5, 0x01c4, 0x01c7, /* U+01C0 */
	0x01c8, 0x01c7, 0x01ca, 0x01cb, 0x01ca, 0x01cd, 0x01cd, 0x01cf, /* U+01C8 */
	0x01cf, 0x01d1, 0x01d1, 0x01d3, 0x01d3, 0x01d5, 0x01d5, 0x01d7, /* U+01D0 */
	0x01d7, 0x01d9, 0x01d9, 0x01db, 0x01db, 0x018e, 0x01de, 0x01de, /* U+01D8 */
	0x01e0, 0x01e0, 0x01e2, 0x01e2, 0x01e4, 0x01e4, 0x01e6, 0x01e6, /* U+01E0 */
	0x01e8, 0x01e8, 0x01ea, 0x01ea, 0x01ec, 0x01ec, 0x01ee, 0x01ee, /* U+01E8 */
	0x01f0, 0x01f1, 0x01f2, 0x01f1, 0x01f4, 0x01f4, 0x01f6, 0x01f7, /* U+01F0 */
	0x01f8, 0x01f8, 0x01fa, 0x01fa, 0x01fc, 0x01fc, 0x01fe, 0x01fe, /* U+01F8 */
	0x0200, 0x0200, 0x0202, 0x0202, 0x0204, 0x0204, 0x0206, 0x0206, /* U+0200 */
	0x0208, 0x0208, 0x020a, 0x020a, 0x020c, 0x020c, 0x020e, 0x020e, /* U+0208 */
	0x0210, 0x0210, 0x0212, 0x0212, 0x0214, 0x0214, 0x0216, 0x0216, /* U+0210 */
	0x0218, 0x0218, 0x021a, 0x021a, 0x021c, 0x021c, 0x021e, 0x021e, /* U+0218 */
	0x0220, 0x0221, 0x0222, 0x0222, 0x0224, 0x0224, 0x0226, 0x0226, /* U+0220 */
	0x0228, 0x0228, 0x022a, 0x022a, 0x022c, 0x022c, 0x022e, 0x022e, /* U+0228 */
	0x0230, 0x0230, 0x0232, 0x0232, 0x0234, 0x0235, 0x0236, 0x0237, /* U+0230 */
	0x0238, 0x0239, 0x2c65, 0x023b, 0x023b, 0x023d, 0x2c66, 0x023f, /* U+0238 */
	0x0240, 0x0241, 0x0241, 0x0243, 0x0244, 0x0245, 0x0246, 0x0246, /* U+0240 */
	0x0248, 0x0248, 0x024a, 0x024a, 0x024c, 0x024c, 0x024e, 0x024e, /* U+0248 */
	0x0250, 0x0251, 0x0252, 0x0181, 0x0186, 0x0255, 0x0189, 0x018a, /* U+0250 */
	0x0258, 0x018f, 0x025a, 0x0190, 0x025c, 0x025d, 0x025e, 0x025f, /* U+0258 */
	0x0193, 0x0261, 0x0262, 0x0194, 0x0264, 0x0265, 0x0266, 0x0267, /* U+0260 */
	0x0197, 0x0196, 0x026a, 0x2c62, 0x026c, 0x026d, 0x026e, 0x019c, /* U+0268 */
	0x0270, 0x0271, 0x019d, 0x0273, 0x0274, 0x019f, 0x0276, 0x0277, /* U+0270 */
	0x0278, 0x0279, 0x027a, 0x027b, 0x027c, 0x2c64, 0x027e, 0x027f, /* U+0278 */
	0x01a6, 0x0281, 0x0282, 0x01a9, 0x0284, 0x0285, 0x0286, 0x0287, /* U+0280 */
	0x01ae, 0x0244, 0x01b1, 0x01b2, 0x0245, 0x028d, 0x028e, 0x028f, /* U+0288 */
	0x0290, 0x0291, 0x01b7, 0x0293, 0x0294, 0x0295, 0x0296, 0x0297, /* U+0290 */
	0x0298, 0x0299, 0x029a, 0x029b, 0x029c, 0x029d, 0x029e, 0x029f, /* U+0298 */
	0x02a0, 0x02a1, 0x02a2, 0x02a3, 0x02a4, 0x02a5, 0x02a6, 0x02a7, /* U+02A0 */
	0x02a8, 0x02a9, 0x02aa, 0x02ab, 0x02ac, 0x02ad, 0x02ae, 0x02af, /* U+02A8 */
	0x02b0, 0x02b1, 0x02b2, 0x02b3, 0x02b4, 0x02b5, 0x02b6, 0x02b7, /* U+02B0 */
	0x02b8, 0x02b9, 0x02ba, 0x02bb, 0x02bc, 0x02bd, 0x02be, 0x02bf, /* U+02B8 */
	0x02c0, 0x02c1, 0x02c2, 0x02c3, 0x02c4, 0x02c5, 0x02c6, 0x02c7, /* U+02C0 */
	0x02c8, 0x02c9, 0x02ca, 0x02cb, 0x02cc, 0x02cd, 0x02ce, 0x02cf, /* U+02C8 */
	0x02d0, 0x02d1, 0x02d2, 0x02d3, 0x02d4, 0x02d5, 0x02d6, 0x02d7, /* U+02D0 */
	0x02d8, 0x02d9, 0x02da, 0x02db, 0x02dc, 0x02dd, 0x02de, 0x02df, /* U+02D8 */
	0x02e0, 0x02e1, 0x02e2, 0x02e3, 0x02e4, 0x02e5, 0x02e6, 0x02e7, /* U+02E0 */
	0x02e8, 0x02e9, 0x02ea, 0x02eb, 0x02ec, 0x02ed, 0x02ee, 0x02ef, /* U+02E8 */
	0x02f0, 0x02f1, 0x02f2, 0x02f3, 0x02f4, 0x02f5, 0x02f6, 0x02f7, /* U+02F0 */
	0x02f8, 0x02f9, 0x02fa, 0x02fb, 0x02fc, 0x02fd, 0x02fe, 0x02ff, /* U+02F8 */
	0x0300, 0x0301, 0x0302, 0x0303, 0x0304, 0x0305, 0x0306, 0x0307, /* U+0300 */
	0x0308, 0x0309, 0x030a, 0x030b, 0x030c, 0x030d, 0x030e, 0x030f, /* U+0308 */
	0x0310, 0x0311, 0x0312, 0x0313, 0x0314, 0x0315, 0x0316, 0x0317, /* U+0310 */
	0x0318, 0x0319, 0x031a, 0x031b, 0x031c, 0x031d, 0x031e, 0x031f, /* U+0318 */
	0x0320, 0x0321, 0x0322, 0x0323, 0x0324, 0x0325, 0x0326, 0x0327, /* U+0320 */
	0x0328, 0x0329, 0x032a, 0x032b, 0x032c, 0x032d, 0x032e, 0x032f, /* U+0328 */
	0x0330, 0x0331, 0x0332, 0x0333, 0x0334, 0x0335, 0x0336, 0x0337, /* U+0330 */
	0x0338, 0x0339, 0x033a, 0x033b, 0x033c, 0x033d, 0x033e, 0x033f, /* U+0338 */
	0x0340, 0x0341, 0x0342, 0x0343, 0x0344, 0x0345, 0x0346, 0x0347, /* U+0340 */
	0x0348, 0x0349, 0x034a, 0x034b, 0x034c, 0x034d, 0x034e, 0x034f, /* U+0348 */
	0x0350, 0x0351, 0x0352, 0x0353, 0x0354, 0x0355, 0x0356, 0x0357, /* U+0350 */
	0x0358, 0x0359, 0x035a, 0x035b, 0x035c, 0x035d, 0x035e, 0x035f, /* U+0358 */
	0x0360, 0x0361, 0x0362, 0x0363, 0x0364, 0x0365, 0x0366, 0x0367, /* U+0360 */
	0x0368, 0x0369, 0x036a, 0x036b, 0x036c, 0x036d, 0x036e, 0x036f, /* U+0368 */
	0x0370, 0x0371, 0x0372, 0x0373, 0x0374, 0x0375, 0x0376, 0x0377, /* U+0370 */
	0x0378, 0x0379, 0x037a, 0x03fd, 0x03fe, 0x03ff, 0x037e, 0x037f, /* U+0378 */
	0x0380, 0x0381, 0x0382, 0x0383, 0x0384, 0x0385, 0x0386, 0x0387, /* U+0380 */
	0x0388, 0x0389, 0x038a, 0x038b, 0x038c, 0x038d, 0x038e, 0x038f, /* U+0388 */
	0x0390, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397, /* U+0390 */
	0x0398, 0x0399, 0x039a, 0x039b, 0x039c, 0x039d, 0x039e, 0x039f, /* U+0398 */
	0x03a0, 0x03a1, 0x03a2, 0x03a3, 0x03a4, 0x03a5, 0x03a6, 0x03a7, /* U+03A0 */
	0x03a8, 0x03a9, 0x03aa, 0x03ab, 0x0386, 0x0388, 0x0389, 0x038a, /* U+03A8 */
	0x03b0, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397, /* U+03B0 */
	0x0398, 0x0399, 0x039a, 0x039b, 0x039c, 0x039d, 0x039e, 0x039f, /* U+03B8 */
	0x03a0, 0x03a1, 0x03a3, 0x03a3, 0x03a4, 0x03a5, 0x03a6, 0x03a7, /* U+03C0 */
	0x03a8, 0x03a9, 0x03aa, 0x03ab, 0x038c, 0x038e, 0x038f, 0x03cf, /* U+03C8 */
	0x03d0, 0x03d1, 0x03d2, 0x03d3, 0x03d4, 0x03d5, 0x03d6, 0x03d7, /* U+03D0 */
	0x03d8, 0x03d8, 0x03da, 0x03da, 0x03dc, 0x03dc, 0x03de, 0x03de, /* U+03D8 */
	0x03e0, 0x03e0, 0x03e2, 0x03e2, 0x03e4, 0x03e4, 0x03e6, 0x03e6, /* U+03E0 */
	0x03e8, 0x03e8, 0x03ea, 0x03ea, 0x03ec, 0x03ec, 0x03ee, 0x03ee, /* U+03E8 */
	0x03f0, 0x03f1, 0x03f9, 0x03f3, 0x03f4, 0x03f5, 0x03f6, 0x03f7, /* U+03F0 */
	0x03f7, 0x03f9, 0x03fa, 0x03fa, 0x03fc, 0x03fd, 0x03fe, 0x03ff, /* U+03F8 */
	0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407, /* U+0400 */
	0x0408, 0x0409, 0x040a, 0x040b, 0x040c, 0x040d, 0x040e, 0x040f, /* U+0408 */
	0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, /* U+0410 */
	0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f, /* U+0418 */
	0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, /* U+0420 */
	0x0428, 0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f, /* U+0428 */
	0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, /* U+0430 */
	0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f, /* U+0438 */
	0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, /* U+0440 */
	0x0428, 0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f, /* U+0448 */
	0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407, /* U+0450 */
	0x0408, 0x0409, 0x040a, 0x040b, 0x040c, 0x040d, 0x040e, 0x040f, /* U+0458 */
	0x0460, 0x0460, 0x0462, 0x0462, 0x0464, 0x0464, 0x0466, 0x0466, /* U+0460 */
	0x0468, 0x0468, 0x046a, 0x046a, 0x046c, 0x046c, 0x046e, 0x046e, /* U+0468 */
	0x0470, 0x0470, 0x0472, 0x0472, 0x0474, 0x0474, 0x0476, 0x0476, /* U+0470 */
	0x0478, 0x0478, 0x047a, 0x047a, 0x047c, 0x047c, 0x047e, 0x047e, /* U+0478 */
	0x0480, 0x0480, 0x0482, 0x0483, 0x0484, 0x0485, 0x0486, 0x0487, /* U+0480 */
	0x0488, 0x0489, 0x048a, 0x048a, 0x048c, 0x048c, 0x048e, 0x048e, /* U+0488 */
	0x0490, 0x0490, 0x0492, 0x0492, 0x0494, 0x0494, 0x0496, 0x0496, /* U+0490 */
	0x0498, 0x0498, 0x049a, 0x049a, 0x049c, 0x049c, 0x049e, 0x049e, /* U+0498 */
	0x04a0, 0x04a0, 0x04a2, 0x04a2, 0x04a4, 0x04a4, 0x04a6, 0x04a6, /* U+04A0 */
	0x04a8, 0x04a8, 0x04aa, 0x04aa, 0x04ac, 0x04ac, 0x04ae, 0x04ae, /* U+04A8 */
	0x04b0, 0x04b0, 0x04b2, 0x04b2, 0x04b4, 0x04b4, 0x04b6, 0x04b6, /* U+04B0 */
	0x04b8, 0x04b8, 0x04ba, 0x04ba, 0x04bc, 0x04bc, 0x04be, 0x04be, /* U+04B8 */
	0x04c0, 0x04c1, 0x04c1, 0x04c3, 0x04c3, 0x04c5, 0x04c5, 0x04c7, /* U+04C0 */
	0x04c7, 0x04c9, 0x04c9, 0x04cb, 0x04cb, 0x04cd, 0x04cd, 0x04c0, /* U+04C8 */
	0x04d0, 0x04d0, 0x04d2, 0x04d2, 0x04d4, 0x04d4, 0x04d6, 0x04d6, /* U+04D0 */
	0x04d8, 0x04d8, 0x04da, 0x04da, 0x04dc, 0x04dc, 0x04de, 0x04de, /* U+04D8 */
	0x04e0, 0x04e0, 0x04e2, 0x04e2, 0x04e4, 0x04e4, 0x04e6, 0x04e6, /* U+04E0 */
	0x04e8, 0x04e8, 0x04ea, 0x04ea, 0x04ec, 0x04ec, 0x04ee, 0x04ee, /* U+04E8 */
	0x04f0, 0x04f0, 0x04f2, 0x04f2, 0x04f4, 0x04f4, 0x04f6, 0x04f6, /* U+04F0 */
	0x04f8, 0x04f8, 0x04fa, 0x04fa, 0x04fc, 0x04fc, 0x04fe, 0x04fe, /* U+04F8 */
	0x0500, 0x0500, 0x0502, 0x0502, 0x0504, 0x0504, 0x0506, 0x0506, /* U+0500 */
	0x0508, 0x0508, 0x050a, 0x050a, 0x050c, 0x050c, 0x050e, 0x050e, /* U+0508 */
	0x0510, 0x0510, 0x0512, 0x0512, 0x0514, 0x0515, 0x0516, 0x0517, /* U+0510 */
	0x0518, 0x0519, 0x051a, 0x051b, 0x051c, 0x051d, 0x051e, 0x051f, /* U+0518 */
	0x0520, 0x0521, 0x0522, 0x0523, 0x0524, 0x0525, 0x0526, 0x0527, /* U+0520 */
	0x0528, 0x0529, 0x052a, 0x052b, 0x052c, 0x052d, 0x052e, 0x052f, /* U+0528 */
	0x0530, 0x0531, 0x0532, 0x0533, 0x0534, 0x0535, 0x0536, 0x0537, /* U+0530 */
	0x0538, 0x0539, 0x053a, 0x053b, 0x053c, 0x053d, 0x053e, 0x053f, /* U+0538 */
	0x0540, 0x0541, 0x0542, 0x0543, 0x0544, 0x0545, 0x0546, 0x0547, /* U+0540 */
	0x0548, 0x0549, 0x054a, 0x054b, 0x054c, 0x054d, 0x054e, 0x054f, /* U+0548 */
	0x0550, 0x0551, 0x0552, 0x0553, 0x0554, 0x0555, 0x0556, 0x0557, /* U+0550 */
	0x0558, 0x0559, 0x055a, 0x055b, 0x055c, 0x055d, 0x055e, 0x055f, /* U+0558 */
	0x0560, 0x0531, 0x0532, 0x0533, 0x0534, 0x0535, 0x0536, 0x0537, /* U+0560 */
	0x0538, 0x0539, 0x053a, 0x053b, 0x053c, 0x053d, 0x053e, 0x053f, /* U+0568 */
	0x0540, 0x0541, 0x0542, 0x0543, 0x0544, 0x0545, 0x0546, 0x0547, /* U+0570 */
	0x0548, 0x0549, 0x054a, 0x054b, 0x054c, 0x054d, 0x054e, 0x054f, /* U+0578 */
	0x0550, 0x0551, 0x0552, 0x0553, 0x0554, 0x0555, 0x0556, /* U+0580 */
	/* U+0587..U+1D7C: identity run. */
	0xffff, 0x17f6,
	0x2c63, 0x1d7e, 0x1d7f, 0x1d80, 0x1d81, 0x1d82, 0x1d83, 0x1d84, /* U+1D7D */
	0x1d85, 0x1d86, 0x1d87, 0x1d88, 0x1d89, 0x1d8a, 0x1d8b, 0x1d8c, /* U+1D85 */
	0x1d8d, 0x1d8e, 0x1d8f, 0x1d90, 0x1d91, 0x1d92, 0x1d93, 0x1d94, /* U+1D8D */
	0x1d95, 0x1d96, 0x1d97, 0x1d98, 0x1d99, 0x1d9a, 0x1d9b, 0x1d9c, /* U+1D95 */
	0x1d9d, 0x1d9e, 0x1d9f, 0x1da0, 0x1da1, 0x1da2, 0x1da3, 0x1da4, /* U+1D9D */
	0x1da5, 0x1da6, 0x1da7, 0x1da8, 0x1da9, 0x1daa, 0x1dab, 0x1dac, /* U+1DA5 */
	0x1dad, 0x1dae, 0x1daf, 0x1db0, 0x1db1, 0x1db2, 0x1db3, 0x1db4, /* U+1DAD */
	0x1db5, 0x1db6, 0x1db7, 0x1db8, 0x1db9, 0x1dba, 0x1dbb, 0x1dbc, /* U+1DB5 */
	0x1dbd, 0x1dbe, 0x1dbf, 0x1dc0, 0x1dc1, 0x1dc2, 0x1dc3, 0x1dc4, /* U+1DBD */
	0x1dc5, 0x1dc6, 0x1dc7, 0x1dc8, 0x1dc9, 0x1dca, 0x1dcb, 0x1dcc, /* U+1DC5 */
	0x1dcd, 0x1dce, 0x1dcf, 0x1dd0, 0x1dd1, 0x1dd2, 0x1dd3, 0x1dd4, /* U+1DCD */
	0x1dd5, 0x1dd6, 0x1dd7, 0x1dd8, 0x1dd9, 0x1dda, 0x1ddb, 0x1ddc, /* U+1DD5 */
	0x1ddd, 0x1dde, 0x1ddf, 0x1de0, 0x1de1, 0x1de2, 0x1de3, 0x1de4, /* U+1DDD */
	0x1de5, 0x1de6, 0x1de7, 0x1de8, 0x1de9, 0x1dea, 0x1deb, 0x1dec, /* U+1DE5 */
	0x1ded, 0x1dee, 0x1def, 0x1df0, 0x1df1, 0x1df2, 0x1df3, 0x1df4, /* U+1DED */
	0x1df5, 0x1df6, 0x1df7, 0x1df8, 0x1df9, 0x1dfa, 0x1dfb, 0x1dfc, /* U+1DF5 */
	0x1dfd, 0x1dfe, 0x1dff, 0x1e00, 0x1e00, 0x1e02, 0x1e02, 0x1e04, /* U+1DFD */
	0x1e04, 0x1e06, 0x1e06, 0x1e08, 0x1e08, 0x1e0a, 0x1e0a, 0x1e0c, /* U+1E05 */
	0x1e0c, 0x1e0e, 0x1e0e, 0x1e10, 0x1e10, 0x1e12, 0x1e12, 0x1e14, /* U+1E0D */
	0x1e14, 0x1e16, 0x1e16, 0x1e18, 0x1e18, 0x1e1a, 0x1e1a, 0x1e1c, /* U+1E15 */
	0x1e1c, 0x1e1e, 0x1e1e, 0x1e20, 0x1e20, 0x1e22, 0x1e22, 0x1e24, /* U+1E1D */
	0x1e24, 0x1e26, 0x1e26, 0x1e28, 0x1e28, 0x1e2a, 0x1e2a, 0x1e2c, /* U+1E25 */
	0x1e2c, 0x1e2e, 0x1e2e, 0x1e30, 0x1e30, 0x1e32, 0x1e32, 0x1e34, /* U+1E2D */
	0x1e34, 0x1e36, 0x1e36, 0x1e38, 0x1e38, 0x1e3a, 0x1e3a, 0x1e3c, /* U+1E35 */
	0x1e3c, 0x1e3e, 0x1e3e, 0x1e40, 0x1e40, 0x1e42, 0x1e42, 0x1e44, /* U+1E3D */
	0x1e44, 0x1e46, 0x1e46, 0x1e48, 0x1e48, 0x1e4a, 0x1e4a, 0x1e4c, /* U+1E45 */
	0x1e4c, 0x1e4e, 0x1e4e, 0x1e50, 0x1e50, 0x1e52, 0x1e52, 0x1e54, /* U+1E4D */
	0x1e54, 0x1e56, 0x1e56, 0x1e58, 0x1e58, 0x1e5a, 0x1e5a, 0x1e5c, /* U+1E55 */
	0x1e5c, 0x1e5e, 0x1e5e, 0x1e60, 0x1e60, 0x1e62, 0x1e62, 0x1e64, /* U+1E5D */
	0x1e64, 0x1e66, 0x1e66, 0x1e68, 0x1e68, 0x1e6a, 0x1e6a, 0x1e6c, /* U+1E65 */
	0x1e6c, 0x1e6e, 0x1e6e, 0x1e70, 0x1e70, 0x1e72, 0x1e72, 0x1e74, /* U+1E6D */
	0x1e74, 0x1e76, 0x1e76, 0x1e78, 0x1e78, 0x1e7a, 0x1e7a, 0x1e7c, /* U+1E75 */
	0x1e7c, 0x1e7e, 0x1e7e, 0x1e80, 0x1e80, 0x1e82, 0x1e82, 0x1e84, /* U+1E7D */
	0x1e84, 0x1e86, 0x1e86, 0x1e88, 0x1e88, 0x1e8a, 0x1e8a, 0x1e8c, /* U+1E85 */
	0x1e8c, 0x1e8e, 0x1e8e, 0x1e90, 0x1e90, 0x1e92, 0x1e92, 0x1e94, /* U+1E8D */
	0x1e94, 0x1e96, 0x1e97, 0x1e98, 0x1e99, 0x1e9a, 0x1e9b, 0x1e9c, /* U+1E95 */
	0x1e9d, 0x1e9e, 0x1e9f, 0x1ea0, 0x1ea0, 0x1ea2, 0x1ea2, 0x1ea4, /* U+1E9D */
	0x1ea4, 0x1ea6, 0x1ea6, 0x1ea8, 0x1ea8, 0x1eaa, 0x1eaa, 0x1eac, /* U+1EA5 */
	0x1eac, 0x1eae, 0x1eae, 0x1eb0, 0x1eb0, 0x1eb2, 0x1eb2, 0x1eb4, /* U+1EAD */
	0x1eb4, 0x1eb6, 0x1eb6, 0x1eb8, 0x1eb8, 0x1eba, 0x1eba, 0x1ebc, /* U+1EB5 */
	0x1ebc, 0x1ebe, 0x1ebe, 0x1ec0, 0x1ec0, 0x1ec2, 0x1ec2, 0x1ec4, /* U+1EBD */
	0x1ec4, 0x1ec6, 0x1ec6, 0x1ec8, 0x1ec8, 0x1eca, 0x1eca, 0x1ecc, /* U+1EC5 */
	0x1ecc, 0x1ece, 0x1ece, 0x1ed0, 0x1ed0, 0x1ed2, 0x1ed2, 0x1ed4, /* U+1ECD */
	0x1ed4, 0x1ed6, 0x1ed6, 0x1ed8, 0x1ed8, 0x1eda, 0x1eda, 0x1edc, /* U+1ED5 */
	0x1edc, 0x1ede, 0x1ede, 0x1ee0, 0x1ee0, 0x1ee2, 0x1ee2, 0x1ee4, /* U+1EDD */
	0x1ee4, 0x1ee6, 0x1ee6, 0x1ee8, 0x1ee8, 0x1eea, 0x1eea, 0x1eec, /* U+1EE5 */
	0x1eec, 0x1eee, 0x1eee, 0x1ef0, 0x1ef0, 0x1ef2, 0x1ef2, 0x1ef4, /* U+1EED */
	0x1ef4, 0x1ef6, 0x1ef6, 0x1ef8, 0x1ef8, 0x1efa, 0x1efb, 0x1efc, /* U+1EF5 */
	0x1efd, 0x1efe, 0x1eff, 0x1f08, 0x1f09, 0x1f0a, 0x1f0b, 0x1f0c, /* U+1EFD */
	0x1f0d, 0x1f0e, 0x1f0f, 0x1f08, 0x1f09, 0x1f0a, 0x1f0b, 0x1f0c, /* U+1F05 */
	0x1f0d, 0x1f0e, 0x1f0f, 0x1f18, 0x1f19, 0x1f1a, 0x1f1b, 0x1f1c, /* U+1F0D */
	0x1f1d, 0x1f16, 0x1f17, 0x1f18, 0x1f19, 0x1f1a, 0x1f1b, 0x1f1c, /* U+1F15 */
	0x1f1d, 0x1f1e, 0x1f1f, 0x1f28, 0x1f29, 0x1f2a, 0x1f2b, 0x1f2c, /* U+1F1D */
	0x1f2d, 0x1f2e, 0x1f2f, 0x1f28, 0x1f29, 0x1f2a, 0x1f2b, 0x1f2c, /* U+1F25 */
	0x1f2d, 0x1f2e, 0x1f2f, 0x1f38, 0x1f39, 0x1f3a, 0x1f3b, 0x1f3c, /* U+1F2D */
	0x1f3d, 0x1f3e, 0x1f3f, 0x1f38, 0x1f39, 0x1f3a, 0x1f3b, 0x1f3c, /* U+1F35 */
	0x1f3d, 0x1f3e, 0x1f3f, 0x1f48, 0x1f49, 0x1f4a, 0x1f4b, 0x1f4c, /* U+1F3D */
	0x1f4d, 0x1f46, 0x1f47, 0x1f48, 0x1f49, 0x1f4a, 0x1f4b, 0x1f4c, /* U+1F45 */
	0x1f4d, 0x1f4e, 0x1f4f, 0x1f50, 0x1f59, 0x1f52, 0x1f5b, 0x1f54, /* U+1F4D */
	0x1f5d, 0x1f56, 0x1f5f, 0x1f58, 0x1f59, 0x1f5a, 0x1f5b, 0x1f5c, /* U+1F55 */
	0x1f5d, 0x1f5e, 0x1f5f, 0x1f68, 0x1f69, 0x1f6a, 0x1f6b, 0x1f6c, /* U+1F5D */
	0x1f6d, 0x1f6e, 0x1f6f, 0x1f68, 0x1f69, 0x1f6a, 0x1f6b, 0x1f6c, /* U+1F65 */
	0x1f6d, 0x1f6e, 0x1f6f, 0x1fba, 0x1fbb, 0x1fc8, 0x1fc9, 0x1fca, /* U+1F6D */
	0x1fcb, 0x1fda, 0x1fdb, 0x1ff8, 0x1ff9, 0x1fea, 0x1feb, 0x1ffa, /* U+1F75 */
	0x1ffb, 0x1f7e, 0x1f7f, 0x1f88, 0x1f89, 0x1f8a, 0x1f8b, 0x1f8c, /* U+1F7D */
	0x1f8d, 0x1f8e, 0x1f8f, 0x1f88, 0x1f89, 0x1f8a, 0x1f8b, 0x1f8c, /* U+1F85 */
	0x1f8d, 0x1f8e, 0x1f8f, 0x1f98, 0x1f99, 0x1f9a, 0x1f9b, 0x1f9c, /* U+1F8D */
	0x1f9d, 0x1f9e, 0x1f9f, 0x1f98, 0x1f99, 0x1f9a, 0x1f9b, 0x1f9c, /* U+1F95 */
	0x1f9d, 0x1f9e, 0x1f9f, 0x1fa8, 0x1fa9, 0x1faa, 0x1fab, 0x1fac, /* U+1F9D */
	0x1fad, 0x1fae, 0x1faf, 0x1fa8, 0x1fa9, 0x1faa, 0x1fab, 0x1fac, /* U+1FA5 */
	0x1fad, 0x1fae, 0x1faf, 0x1fb8, 0x1fb9, 0x1fb2, 0x1fbc, 0x1fb4, /* U+1FAD */
	0x1fb5, 0x1fb6, 0x1fb7, 0x1fb8, 0x1fb9, 0x1fba, 0x1fbb, 0x1fbc, /* U+1FB5 */
	0x1fbd, 0x1fbe, 0x1fbf, 0x1fc0, 0x1fc1, 0x1fc2, 0x1fc3, 0x1fc4, /* U+1FBD */
	0x1fc5, 0x1fc6, 0x1fc7, 0x1fc8, 0x1fc9, 0x1fca, 0x1fcb, 0x1fc3, /* U+1FC5 */
	0x1fcd, 0x1fce, 0x1fcf, 0x1fd8, 0x1fd9, 0x1fd2, 0x1fd3, 0x1fd4, /* U+1FCD */
	0x1fd5, 0x1fd6, 0x1fd7, 0x1fd8, 0x1fd9, 0x1fda, 0x1fdb, 0x1fdc, /* U+1FD5 */
	0x1fdd, 0x1fde, 0x1fdf, 0x1fe8, 0x1fe9, 0x1fe2, 0x1fe3, 0x1fe4, /* U+1FDD */
	0x1fec, 0x1fe6, 0x1fe7, 0x1fe8, 0x1fe9, 0x1fea, 0x1feb, 0x1fec, /* U+1FE5 */
	0x1fed, 0x1fee, 0x1fef, 0x1ff0, 0x1ff1, 0x1ff2, 0x1ff3, 0x1ff4, /* U+1FED */
	0x1ff5, 0x1ff6, 0x1ff7, 0x1ff8, 0x1ff9, 0x1ffa, 0x1ffb, 0x1ff3, /* U+1FF5 */
	0x1ffd, 0x1ffe, 0x1fff, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004, /* U+1FFD */
	0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200a, 0x200b, 0x200c, /* U+2005 */
	0x200d, 0x200e, 0x200f, 0x2010, 0x2011, 0x2012, 0x2013, 0x2014, /* U+200D */
	0x2015, 0x2016, 0x2017, 0x2018, 0x2019, 0x201a, 0x201b, 0x201c, /* U+2015 */
	0x201d, 0x201e, 0x201f, 0x2020, 0x2021, 0x2022, 0x2023, 0x2024, /* U+201D */
	0x2025, 0x2026, 0x2027, 0x2028, 0x2029, 0x202a, 0x202b, 0x202c, /* U+2025 */
	0x202d, 0x202e, 0x202f, 0x2030, 0x2031, 0x2032, 0x2033, 0x2034, /* U+202D */
	0x2035, 0x2036, 0x2037, 0x2038, 0x2039, 0x203a, 0x203b, 0x203c, /* U+2035 */
	0x203d, 0x203e, 0x203f, 0x2040, 0x2041, 0x2042, 0x2043, 0x2044, /* U+203D */
	0x2045, 0x2046, 0x2047, 0x2048, 0x2049, 0x204a, 0x204b, 0x204c, /* U+2045 */
	0x204d, 0x204e, 0x204f, 0x2050, 0x2051, 0x2052, 0x2053, 0x2054, /* U+204D */
	0x2055, 0x2056, 0x2057, 0x2058, 0x2059, 0x205a, 0x205b, 0x205c, /* U+2055 */
	0x205d, 0x205e, 0x205f, 0x2060, 0x2061, 0x2062, 0x2063, 0x2064, /* U+205D */
	0x2065, 0x2066, 0x2067, 0x2068, 0x2069, 0x206a, 0x206b, 0x206c, /* U+2065 */
	0x206d, 0x206e, 0x206f, 0x2070, 0x2071, 0x2072, 0x2073, 0x2074, /* U+206D */
	0x2075, 0x2076, 0x2077, 0x2078, 0x2079, 0x207a, 0x207b, 0x207c, /* U+2075 */
	0x207d, 0x207e, 0x207f, 0x2080, 0x2081, 0x2082, 0x2083, 0x2084, /* U+207D */
	0x2085, 0x2086, 0x2087, 0x2088, 0x2089, 0x208a, 0x208b, 0x208c, /* U+2085 */
	0x208d, 0x208e, 0x208f, 0x2090, 0x2091, 0x2092, 0x2093, 0x2094, /* U+208D */
	0x2095, 0x2096, 0x2097, 0x2098, 0x2099, 0x209a, 0x209b, 0x209c, /* U+2095 */
	0x209d, 0x209e, 0x209f, 0x20a0, 0x20a1, 0x20a2, 0x20a3, 0x20a4, /* U+209D */
	0x20a5, 0x20a6, 0x20a7, 0x20a8, 0x20a9, 0x20aa, 0x20ab, 0x20ac, /* U+20A5 */
	0x20ad, 0x20ae, 0x20af, 0x20b0, 0x20b1, 0x20b2, 0x20b3, 0x20b4, /* U+20AD */
	0x20b5, 0x20b6, 0x20b7, 0x20b8, 0x20b9, 0x20ba, 0x20bb, 0x20bc, /* U+20B5 */
	0x20bd, 0x20be, 0x20bf, 0x20c0, 0x20c1, 0x20c2, 0x20c3, 0x20c4, /* U+20BD */
	0x20c5, 0x20c6, 0x20c7, 0x20c8, 0x20c9, 0x20ca, 0x20cb, 0x20cc, /* U+20C5 */
	0x20cd, 0x20ce, 0x20cf, 0x20d0, 0x20d1, 0x20d2, 0x20d3, 0x20d4, /* U+20CD */
	0x20d5, 0x20d6, 0x20d7, 0x20d8, 0x20d9, 0x20da, 0x20db, 0x20dc, /* U+20D5 */
	0x20dd, 0x20de, 0x20df, 0x20e0, 0x20e1, 0x20e2, 0x20e3, 0x20e4, /* U+20DD */
	0x20e5, 0x20e6, 0x20e7, 0x20e8, 0x20e9, 0x20ea, 0x20eb, 0x20ec, /* U+20E5 */
	0x20ed, 0x20ee, 0x20ef, 0x20f0, 0x20f1, 0x20f2, 0x20f3, 0x20f4, /* U+20ED */
	0x20f5, 0x20f6, 0x20f7, 0x20f8, 0x20f9, 0x20fa, 0x20fb, 0x20fc, /* U+20F5 */
	0x20fd, 0x20fe, 0x20ff, 0x2100, 0x2101, 0x2102, 0x2103, 0x2104, /* U+20FD */
	0x2105, 0x2106, 0x2107, 0x2108, 0x2109, 0x210a, 0x210b, 0x210c, /* U+2105 */
	0x210d, 0x210e, 0x210f, 0x2110, 0x2111, 0x2112, 0x2113, 0x2114, /* U+210D */
	0x2115, 0x2116, 0x2117, 0x2118, 0x2119, 0x211a, 0x211b, 0x211c, /* U+2115 */
	0x211d, 0x211e, 0x211f, 0x2120, 0x2121, 0x2122, 0x2123, 0x2124, /* U+211D */
	0x2125, 0x2126, 0x2127, 0x2128, 0x2129, 0x212a, 0x212b, 0x212c, /* U+2125 */
	0x212d, 0x212e, 0x212f, 0x2130, 0x2131, 0x2132, 0x2133, 0x2134, /* U+212D */
	0x2135, 0x2136, 0x2137, 0x2138, 0x2139, 0x213a, 0x213b, 0x213c, /* U+2135 */
	0x213d, 0x213e, 0x213f, 0x2140, 0x2141, 0x2142, 0x2143, 0x2144, /* U+213D */
	0x2145, 0x2146, 0x2147, 0x2148, 0x2149, 0x214a, 0x214b, 0x214c, /* U+2145 */
	0x214d, 0x2132, 0x214f, 0x2150, 0x2151, 0x2152, 0x2153, 0x2154, /* U+214D */
	0x2155, 0x2156, 0x2157, 0x2158, 0x2159, 0x215a, 0x215b, 0x215c, /* U+2155 */
	0x215d, 0x215e, 0x215f, 0x2160, 0x2161, 0x2162, 0x2163, 0x2164, /* U+215D */
	0x2165, 0x2166, 0x2167, 0x2168, 0x2169, 0x216a, 0x216b, 0x216c, /* U+2165 */
	0x216d, 0x216e, 0x216f, 0x2160, 0x2161, 0x2162, 0x2163, 0x2164, /* U+216D */
	0x2165, 0x2166, 0x2167, 0x2168, 0x2169, 0x216a, 0x216b, 0x216c, /* U+2175 */
	0x216d, 0x216e, 0x216f, 0x2180, 0x2181, 0x2182, 0x2183, 0x2183, /* U+217D */
	/* U+2185..U+24CF: identity run. */
	0xffff, 0x034b,
	0x24b6, 0x24b7, 0x24b8, 0x24b9, 0x24ba, 0x24bb, 0x24bc, 0x24bd, /* U+24D0 */
	0x24be, 0x24bf, 0x24c0, 0x24c1, 0x24c2, 0x24c3, 0x24c4, 0x24c5, /* U+24D8 */
	0x24c6, 0x24c7, 0x24c8, 0x24c9, 0x24ca, 0x24cb, 0x24cc, 0x24cd, /* U+24E0 */
	0x24ce, 0x24cf, /* U+24E8 */
	/* U+24EA..U+2C2F: identity run. */
	0xffff, 0x0746,
	0x2c00, 0x2c01, 0x2c02, 0x2c03, 0x2c04, 0x2c05, 0x2c06, 0x2c07, /* U+2C30 */
	0x2c08, 0x2c09, 0x2c0a, 0x2c0b, 0x2c0c, 0x2c0d, 0x2c0e, 0x2c0f, /* U+2C38 */
	0x2c10, 0x2c11, 0x2c12, 0x2c13, 0x2c14, 0x2c15, 0x2c16, 0x2c17, /* U+2C40 */
	0x2c18, 0x2c19, 0x2c1a, 0x2c1b, 0x2c1c, 0x2c1d, 0x2c1e, 0x2c1f, /* U+2C48 */
	0x2c20, 0x2c21, 0x2c22, 0x2c23, 0x2c24, 0x2c25, 0x2c26, 0x2c27, /* U+2C50 */
	0x2c28, 0x2c29, 0x2c2a, 0x2c2b, 0x2c2c, 0x2c2d, 0x2c2e, 0x2c5f, /* U+2C58 */
	0x2c60, 0x2c60, 0x2c62, 0x2c63, 0x2c64, 0x2c65, 0x2c66, 0x2c67, /* U+2C60 */
	0x2c67, 0x2c69, 0x2c69, 0x2c6b, 0x2c6b, 0x2c6d, 0x2c6e, 0x2c6f, /* U+2C68 */
	0x2c70, 0x2c71, 0x2c72, 0x2c73, 0x2c74, 0x2c75, 0x2c75, 0x2c77, /* U+2C70 */
	0x2c78, 0x2c79, 0x2c7a, 0x2c7b, 0x2c7c, 0x2c7d, 0x2c7e, 0x2c7f, /* U+2C78 */
	0x2c80, 0x2c80, 0x2c82, 0x2c82, 0x2c84, 0x2c84, 0x2c86, 0x2c86, /* U+2C80 */
	0x2c88, 0x2c88, 0x2c8a, 0x2c8a, 0x2c8c, 0x2c8c, 0x2c8e, 0x2c8e, /* U+2C88 */
	0x2c90, 0x2c90, 0x2c92, 0x2c92, 0x2c94, 0x2c94, 0x2c96, 0x2c96, /* U+2C90 */
	0x2c98, 0x2c98, 0x2c9a, 0x2c9a, 0x2c9c, 0x2c9c, 0x2c9e, 0x2c9e, /* U+2C98 */
	0x2ca0, 0x2ca0, 0x2ca2, 0x2ca2, 0x2ca4, 0x2ca4, 0x2ca6, 0x2ca6, /* U+2CA0 */
	0x2ca8, 0x2ca8, 0x2caa, 0x2caa, 0x2cac, 0x2cac, 0x2cae, 0x2cae, /* U+2CA8 */
	0x2cb0, 0x2cb0, 0x2cb2, 0x2cb2, 0x2cb4, 0x2cb4, 0x2cb6, 0x2cb6, /* U+2CB0 */
	0x2cb8, 0x2cb8, 0x2cba, 0x2cba, 0x2cbc, 0x2cbc, 0x2cbe, 0x2cbe, /* U+2CB8 */
	0x2cc0, 0x2cc0, 0x2cc2, 0x2cc2, 0x2cc4, 0x2cc4, 0x2cc6, 0x2cc6, /* U+2CC0 */
	0x2cc8, 0x2cc8, 0x2cca, 0x2cca, 0x2ccc, 0x2ccc, 0x2cce, 0x2cce, /* U+2CC8 */
	0x2cd0, 0x2cd0, 0x2cd2, 0x2cd2, 0x2cd4, 0x2cd4, 0x2cd6, 0x2cd6, /* U+2CD0 */
	0x2cd8, 0x2cd8, 0x2cda, 0x2cda, 0x2cdc, 0x2cdc, 0x2cde, 0x2cde, /* U+2CD8 */
	0x2ce0, 0x2ce0, 0x2ce2, 0x2ce2, 0x2ce4, 0x2ce5, 0x2ce6, 0x2ce7, /* U+2CE0 */
	0x2ce8, 0x2ce9, 0x2cea, 0x2ceb, 0x2cec, 0x2ced, 0x2cee, 0x2cef, /* U+2CE8 */
	0x2cf0, 0x2cf1, 0x2cf2, 0x2cf3, 0x2cf4, 0x2cf5, 0x2cf6, 0x2cf7, /* U+2CF0 */
	0x2cf8, 0x2cf9, 0x2cfa, 0x2cfb, 0x2cfc, 0x2cfd, 0x2cfe, 0x2cff, /* U+2CF8 */
	0x10a0, 0x10a1, 0x10a2, 0x10a3, 0x10a4, 0x10a5, 0x10a6, 0x10a7, /* U+2D00 */
	0x10a8, 0x10a9, 0x10aa, 0x10ab, 0x10ac, 0x10ad, 0x10ae, 0x10af, /* U+2D08 */
	0x10b0, 0x10b1, 0x10b2, 0x10b3, 0x10b4, 0x10b5, 0x10b6, 0x10b7, /* U+2D10 */
	0x10b8, 0x10b9, 0x10ba, 0x10bb, 0x10bc, 0x10bd, 0x10be, 0x10bf, /* U+2D18 */
	0x10c0, 0x10c1, 0x10c2, 0x10c3, 0x10c4, 0x10c5, /* U+2D20 */
	/* U+2D26..U+FF40: identity run. */
	0xffff, 0xd21b,
	0xff21, 0xff22, 0xff23, 0xff24, 0xff25, 0xff26, 0xff27, 0xff28, /* U+FF41 */
	0xff29, 0xff2a, 0xff2b, 0xff2c, 0xff2d, 0xff2e, 0xff2f, 0xff30, /* U+FF49 */
	0xff31, 0xff32, 0xff33, 0xff34, 0xff35, 0xff36, 0xff37, 0xff38, /* U+FF51 */
	0xff39, 0xff3a, 0xff5b, 0xff5c, 0xff5d, 0xff5e, 0xff5f, 0xff60, /* U+FF59 */
	0xff61, 0xff62, 0xff63, 0xff64, 0xff65, 0xff66, 0xff67, 0xff68, /* U+FF61 */
	0xff69, 0xff6a, 0xff6b, 0xff6c, 0xff6d, 0xff6e, 0xff6f, 0xff70, /* U+FF69 */
	0xff71, 0xff72, 0xff73, 0xff74, 0xff75, 0xff76, 0xff77, 0xff78, /* U+FF71 */
	0xff79, 0xff7a, 0xff7b, 0xff7c, 0xff7d, 0xff7e, 0xff7f, 0xff80, /* U+FF79 */
	0xff81, 0xff82, 0xff83, 0xff84, 0xff85, 0xff86, 0xff87, 0xff88, /* U+FF81 */
	0xff89, 0xff8a, 0xff8b, 0xff8c, 0xff8d, 0xff8e, 0xff8f, 0xff90, /* U+FF89 */
	0xff91, 0xff92, 0xff93, 0xff94, 0xff95, 0xff96, 0xff97, 0xff98, /* U+FF91 */
	0xff99, 0xff9a, 0xff9b, 0xff9c, 0xff9d, 0xff9e, 0xff9f, 0xffa0, /* U+FF99 */
	0xffa1, 0xffa2, 0xffa3, 0xffa4, 0xffa5, 0xffa6, 0xffa7, 0xffa8, /* U+FFA1 */
	0xffa9, 0xffaa, 0xffab, 0xffac, 0xffad, 0xffae, 0xffaf, 0xffb0, /* U+FFA9 */
	0xffb1, 0xffb2, 0xffb3, 0xffb4, 0xffb5, 0xffb6, 0xffb7, 0xffb8, /* U+FFB1 */
	0xffb9, 0xffba, 0xffbb, 0xffbc, 0xffbd, 0xffbe, 0xffbf, 0xffc0, /* U+FFB9 */
	0xffc1, 0xffc2, 0xffc3, 0xffc4, 0xffc5, 0xffc6, 0xffc7, 0xffc8, /* U+FFC1 */
	0xffc9, 0xffca, 0xffcb, 0xffcc, 0xffcd, 0xffce, 0xffcf, 0xffd0, /* U+FFC9 */
	0xffd1, 0xffd2, 0xffd3, 0xffd4, 0xffd5, 0xffd6, 0xffd7, 0xffd8, /* U+FFD1 */
	0xffd9, 0xffda, 0xffdb, 0xffdc, 0xffdd, 0xffde, 0xffdf, 0xffe0, /* U+FFD9 */
	0xffe1, 0xffe2, 0xffe3, 0xffe4, 0xffe5, 0xffe6, 0xffe7, 0xffe8, /* U+FFE1 */
	0xffe9, 0xffea, 0xffeb, 0xffec, 0xffed, 0xffee, 0xffef, 0xfff0, /* U+FFE9 */
	0xfff1, 0xfff2, 0xfff3, 0xfff4, 0xfff5, 0xfff6, 0xfff7, 0xfff8, /* U+FFF1 */
	0xfff9, 0xfffa, 0xfffb, 0xfffc, 0xfffd, 0xfffe, 0xffff, /* U+FFF9 */
};

static uint8_t exfatUpcaseWriteBuffer[6144];

static const uint32_t fatMaxClusterSizes[5] = {
	32768, 32768, 32768, 32768, 33554432
};

#if FW_DEBUG
uint32_t exfatDefaultRootCluster = 4;
uint32_t exfatDefaultFatLength = 64;
uint8_t exfatDefaultClusterShift = 3;
uint32_t exfatDefaultHeapOffset = 256;
uint32_t exfatDefaultClusterCount = 7885;
uint32_t exfatDefaultFatOffset = 128;
uint64_t exfatDefaultVolumeLength = 63341;
#else
uint32_t exfatDefaultRootCluster = 4;
uint32_t exfatDefaultFatOffset = 128;
uint64_t exfatDefaultVolumeLength = 63341;
uint32_t exfatDefaultHeapOffset = 256;
uint32_t exfatDefaultFatLength = 64;
uint8_t exfatDefaultClusterShift = 3;
uint32_t exfatDefaultClusterCount = 7885;
#endif

#if !FW_DEBUG
static const uint16_t fatDefaultReservedSectors[5] = { 32, 1, 1, 32, 24 };
static const uint32_t fatPartitionSystemIds[4] = { 1, 6, 11, 7 };
#endif

int _fs_util_get_device_info(const char *blockDeviceName, void *buffer,
	unsigned int bufferSize);
int _fs_util_prepare_mbr(const char *blockDeviceName, unsigned char systemId,
	SceMbr *buffer, unsigned int bufferSize, uint64_t volumeSize);
int _fs_util_device_format(const char *blockDeviceName);
int _fs_util_prepare_rm_mbr(const char *blockDeviceName, SceUID file,
	uint64_t volumeSize, unsigned int sectorSize, SceMbr *buffer,
	unsigned int bufferSize);
uint64_t appmgrMbrGetFdiskOffset(const SceSdstorMbr *mbr, unsigned int index);
uint64_t appmgrMbrGetFdiskSize(const SceSdstorMbr *mbr, unsigned int index);
uint64_t appmgrMbrGetPartitionOffset(const SceMbr *mbr, int index);
uint64_t appmgrMbrGetPartitionSize(const SceMbr *mbr, int index);
unsigned int appmgrMbrGetFormatType(const SceMbr *mbr, int index);
unsigned int appmgrMbrGetClusterSize(const SceMbr *mbr, int index);

unsigned int appmgrPackFatDateTime(const SceDateTime *time)
{
	return (uint32_t)time->month << 21 | (uint32_t)time->day << 16 |
	       (uint32_t)time->hour << 11 | (uint32_t)time->minute << 5 |
	       time->second >> 1 | ((uint32_t)time->year - 80) << 25;
}

int appmgrFatFormatDefaultNotification(struct AppmgrFatFormatArguments *arguments)
{
	(void)arguments;
	return 0;
}


unsigned int appmgrPrepareFat32FsInfo(AppmgrFatFormatContext *context,
				    unsigned int bufferOffset)
{
	AppmgrFat32FsInfoSector *sector;
	uint32_t freeClusters;

	sector = (AppmgrFat32FsInfoSector *)
		((uint8_t *)context->arguments->buffer + bufferOffset);
	memset(sector, 0, context->bytesPerSector);
	sector->trailSignature[3] = 0xaa;
	sector->trailSignature[2] = 0x55;
	sector->leadSignature[0] = 'R';
	sector->leadSignature[2] = 'a';
	sector->structureSignature[3] = 'a';
	sector->leadSignature[1] = 'R';
	sector->leadSignature[3] = 'A';
	sector->structureSignature[0] = 'r';
	sector->structureSignature[1] = 'r';
	sector->structureSignature[2] = 'A';
	sector->freeClusterCount[0] = context->clusterCount - 1;
	sector->freeClusterCount[1] = (context->clusterCount - 1) >> 8;
	sector->freeClusterCount[2] = (context->clusterCount - 1) >> 16;
	freeClusters = context->clusterCount - 1;
	sector->nextFreeCluster[0] = 0xff;
	sector->nextFreeCluster[1] = 0xff;
	sector->nextFreeCluster[2] = 0xff;
	sector->nextFreeCluster[3] = 0xff;
	sector->freeClusterCount[3] = freeClusters >> 24;
	return freeClusters;
}

int appmgrPrepareFat32RootCluster(AppmgrFatFormatContext *context)
{
	AppmgrFatFormatArguments *arguments;
	uint64_t fatSpan, byteOffset;
	uint32_t limit, clusterSize, bufferOffset, writeAlignment;
	int result;

	if (context->clusterCount == 0)
		return (int)0x80010016;
	limit = context->clusterCount + 2;
	writeAlignment = context->writeAlignment;
	arguments = context->arguments;
	bufferOffset = context->bytesPerSector;
	clusterSize = arguments->clusterSize;
	fatSpan = (uint64_t)context->fatCount * writeAlignment;
	byteOffset = (uint32_t)(clusterSize * (context->rootCluster - 2)) +
		(uint64_t)(uint32_t)(context->reservedSectors * bufferOffset);
	byteOffset += fatSpan *
		(((uint64_t)writeAlignment - 1 + (uint32_t)(4 * limit)) /
		 writeAlignment);
	if (arguments->bufferSize <= bufferOffset)
		bufferOffset = 0;
	for (;;) {
		result = zero_fill(context, bufferOffset, clusterSize, byteOffset, 1);
		if (result >= 0)
			break;
		clusterSize = arguments->clusterSize;
		context->rootCluster++;
		if (limit <= context->rootCluster)
			break;
		byteOffset += clusterSize;
	}
	context->firstUnusedCluster = context->rootCluster + 1;
	return result;
}

#if FW_DEBUG
static inline unsigned int _request_type(AppmgrFatFormatContext *context,
					 int index)
{
	if (index == 1)
		return context->type;
	if (index == 2)
		return context->alternate.type;
	SceDebugForDriver_1A3F2AA4(1, "%s: unsupported index(%d)\n", __func__, index);
	return 0;
}

static inline unsigned int _cluster_size(AppmgrFatFormatContext *context,
					 int index)
{
	if (index == 1)
		return context->arguments->clusterSize;
	if (index == 2)
		return context->alternate.clusterSize;
	SceDebugForDriver_1A3F2AA4(1, "%s: unsupported index(%d)\n", __func__, index);
	return 0;
}

unsigned int appmgrGetFatUpcaseAllocationSize(AppmgrFatFormatContext *context,
					     int index)
{
	if (_request_type(context, index) != 4)
		return 0;
	return ((_cluster_size(context, index) + 5835) /
		_cluster_size(context, index)) * _cluster_size(context, index);
}

unsigned int appmgrBuildFatClusterChain(AppmgrFatFormatContext *context,
	unsigned int cluster, unsigned int bufferOffset, uint64_t length,
	unsigned int bufferSize)
{
	AppmgrFatFormatArguments *arguments = context->arguments;
	uint8_t *entry = (uint8_t *)arguments->buffer + bufferOffset;
	uint8_t *end = entry + bufferSize;
	uint32_t clusterSize;
	int hasSpace = entry < end;

	if (arguments->clusterSize < length && hasSpace) {
		do {
			cluster++;
			entry[0] = cluster;
			entry[2] = cluster >> 16;
			entry[1] = cluster >> 8;
			entry[3] = cluster >> 24;
			entry += 4;
			clusterSize = arguments->clusterSize;
			hasSpace = entry < end;
			length -= clusterSize;
		} while (clusterSize < length && hasSpace);
	}
	if (length != 0 && hasSpace) {
		if (context->type == 4) {
			cluster++;
			entry[0] = 0xff;
			entry[1] = 0xff;
			entry[2] = 0xff;
			entry[3] = 0xff;
		} else {
			entry[0] = 0xff;
			cluster++;
			entry[1] = 0xff;
			entry[2] = 0xff;
			entry[3] = 0x0f;
		}
	}
	return cluster;
}
#endif

uint32_t _fat32_cluster_count(uint64_t volumeSize, uint32_t clusterSize,
	uint32_t sectorSize, uint64_t startLba, uint32_t reservedAlignment,
	uint32_t fatAlignment)
{
	uint64_t reservedSize, limit, available, quotient, groupSize, groups;
	uint64_t fatCopiesSize = 2 * (uint64_t)fatAlignment;
	uint32_t entriesPerGroup, initialCount, count;
	uint64_t initialFatSize;
#if FW_DEBUG
	const char *message;
#endif

	reservedSize = ((startLba + 32) * sectorSize + reservedAlignment - 1) /
		reservedAlignment * reservedAlignment - startLba * sectorSize;
	limit = (uint64_t)268435446 * clusterSize +
		((1073741791 + (uint64_t)fatAlignment) / fatAlignment) *
		fatCopiesSize + reservedSize;
	if (volumeSize >= limit) {
#if FW_DEBUG
		message = "%s: volume_size(%llu) is too large for FAT32, reserved_alignment[%#x], fat_alignment[%#x], cluster_size[%#x], too_large_size(%llu)\n";
#endif
		goto invalid;
	}
	limit = (uint64_t)65525 * clusterSize +
		((262107 + (uint64_t)fatAlignment) / fatAlignment) *
		fatCopiesSize + reservedSize;
	if (volumeSize < limit) {
#if FW_DEBUG
		message = "%s: volume_size(%llu) is too small for FAT32, reserved_alignment[%#x], fat_alignment[%#x], cluster_size[%#x], minimum_size(%llu)\n";
#endif
		goto invalid;
	}
	entriesPerGroup = fatAlignment >> 2;
	initialCount = entriesPerGroup - 2;
	initialFatSize = (uint32_t)(2 * fatAlignment);
	available = volumeSize - initialFatSize - reservedSize;
	quotient = available / clusterSize;
	if (quotient < initialCount)
		return quotient;
	groupSize = (uint64_t)clusterSize * entriesPerGroup + initialFatSize;
	available -= (uint64_t)initialCount * clusterSize;
	groups = available / groupSize;
	count = initialCount + groups * entriesPerGroup;
	available -= groupSize * groups;
	if (initialFatSize < available)
		count += (available - initialFatSize) / clusterSize;
	return count;
invalid:
#if FW_DEBUG
	SceDebugForDriver_1A3F2AA4(1, message, __func__, volumeSize, reservedAlignment,
		fatAlignment, clusterSize, limit);
#endif
	return 0;
}

uint16_t _fat16_cluster_count(uint64_t volumeSize, uint32_t clusterSize,
	uint32_t sectorSize, uint64_t startLba, uint32_t reservedAlignment,
	uint32_t fatAlignment)
{
	uint64_t reservedSize, limit, available, quotient, groupSize, groups;
	uint64_t fatCopiesSize = 2 * (uint64_t)fatAlignment;
	uint64_t initialFatSize;
	uint32_t rootDirectorySize, entriesPerGroup;
	uint16_t count;
#if FW_DEBUG
	const char *message;
#endif

	rootDirectorySize = ((sectorSize + 16383) / sectorSize) * sectorSize;
	reservedSize = ((startLba + 1) * sectorSize + reservedAlignment - 1) /
		reservedAlignment * reservedAlignment + rootDirectorySize -
		startLba * sectorSize;
	limit = (uint64_t)65525 * clusterSize +
		((131053 + (uint64_t)fatAlignment) / fatAlignment) *
		fatCopiesSize + reservedSize;
	if (volumeSize >= limit) {
#if FW_DEBUG
		message = "%s: volume_size(%llu) is too large for FAT16, reserved_alignment[%#x], fat_alignment[%#x], cluster_size[%#x], too_large_size(%llu)\n";
#endif
		goto invalid;
	}
	limit = (uint64_t)4085 * clusterSize +
		((8173 + (uint64_t)fatAlignment) / fatAlignment) *
		fatCopiesSize + reservedSize;
	if (volumeSize < limit) {
#if FW_DEBUG
		message = "%s: volume_size(%llu) is too small for FAT16, reserved_alignment[%#x], fat_alignment[%#x], cluster_size[%#x], minimum_size(%llu)\n";
#endif
		goto invalid;
	}
	entriesPerGroup = fatAlignment >> 1;
	initialFatSize = (uint32_t)(2 * fatAlignment);
	available = volumeSize - initialFatSize - reservedSize;
	quotient = available / clusterSize;
	count = quotient;
	if (quotient >= (uint32_t)(entriesPerGroup - 2)) {
		groupSize = (uint64_t)clusterSize * entriesPerGroup + initialFatSize;
		available -= (uint64_t)count * clusterSize;
		groups = available / groupSize;
		count += entriesPerGroup * groups;
		available -= groupSize * groups;
		if (initialFatSize < available)
			count += (available - initialFatSize) / clusterSize;
	}
	return count;
invalid:
#if FW_DEBUG
	SceDebugForDriver_1A3F2AA4(1, message, __func__, volumeSize, reservedAlignment,
		fatAlignment, clusterSize, limit);
#endif
	return 0;
}

uint16_t _fat12_cluster_count(uint64_t volumeSize, uint32_t clusterSize,
	uint32_t sectorSize, uint64_t startLba, uint32_t reservedAlignment,
	uint32_t fatAlignment)
{
	uint64_t reservedSize, limit, available, quotient, fatCopiesSize;
	uint32_t rootDirectorySize, entriesPerGroup, remainder, accumulatedRemainder;
	int32_t groupCount;
	uint16_t count;

	rootDirectorySize = ((sectorSize + 16383) / sectorSize) * sectorSize;
	reservedSize = ((startLba + 1) * sectorSize + reservedAlignment - 1) /
		reservedAlignment * reservedAlignment + rootDirectorySize -
		startLba * sectorSize;
	limit = (uint64_t)4085 * clusterSize +
		((6130 + (uint64_t)fatAlignment) / fatAlignment) *
		2 * (uint64_t)fatAlignment + reservedSize;
	if (volumeSize >= limit) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1,
			"%s: volume_size(%llu) is too large for FAT12, reserved_alignment[%#x], fat_alignment[%#x], cluster_size[%#x], too_large_size(%llu)\n",
			__func__, volumeSize, reservedAlignment, fatAlignment,
			clusterSize, limit);
#endif
		return 0;
	}
	fatCopiesSize = (uint32_t)(2 * fatAlignment);
	limit = reservedSize + fatCopiesSize;
	if (volumeSize < limit) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1,
			"%s: volume_size(%llu) is too small for FAT12, reserved_alignment[%#x], fat_alignment[%#x], cluster_size[%#x], minimum_size(%llu)\n",
			__func__, volumeSize, reservedAlignment, fatAlignment,
			clusterSize, limit);
#endif
		return 0;
	}
	entriesPerGroup = 2 * (fatAlignment / 3);
	groupCount = entriesPerGroup - 2;
	remainder = fatAlignment % 3;
	accumulatedRemainder = remainder;
	if (remainder == 2)
		groupCount = entriesPerGroup - 1;
	count = 0;
	available = volumeSize - limit;
	for (;;) {
		quotient = available / (uint64_t)clusterSize;
		if (quotient < (uint64_t)(int64_t)groupCount)
			return count + (uint16_t)quotient;
		accumulatedRemainder += remainder;
		count = (uint32_t)count + (uint32_t)groupCount;
		available -= (uint64_t)(int64_t)groupCount * clusterSize;
		if (fatCopiesSize >= available)
			return count;
		available -= fatCopiesSize;
		switch (accumulatedRemainder) {
		case 2:
			groupCount = entriesPerGroup + 1;
			break;
		case 3:
			accumulatedRemainder = 0;
			groupCount = entriesPerGroup + 1;
			break;
		case 4:
			groupCount = entriesPerGroup + 1;
			break;
		case 6:
			accumulatedRemainder = 0;
			groupCount = entriesPerGroup + 2;
			break;
		default:
			groupCount = entriesPerGroup;
			break;
		}
		if (clusterSize >= available)
			return count;
	}
}

int appmgrSelectFatFormat(uint64_t volumeSize, uint32_t clusterSize,
	uint32_t sectorSize, uint64_t startLba, uint32_t alignment)
{
	if (_fat32_cluster_count(volumeSize, clusterSize, sectorSize, startLba,
				alignment, alignment) != 0)
		return 3;
	if (_fat16_cluster_count(volumeSize, clusterSize, sectorSize, startLba,
				alignment, alignment) != 0)
		return 2;
	return _fat12_cluster_count(volumeSize, clusterSize, sectorSize, startLba,
				   alignment, alignment) != 0;
}

#if FW_DEBUG
int device_fd_write(AppmgrFatFormatContext *context, const void *buffer,
		    unsigned int size, uint64_t byteOffset)
{
	SceOff seekResult;
	int result, error, notificationResult;
	AppmgrFatFormatNotification notification;
	AppmgrFatFormatArguments *arguments;

	seekResult = ksceIoLseek(context->file, byteOffset, SCE_SEEK_SET);
	if (seekResult < 0) {
		SceDebugForDriver_1A3F2AA4(1,
			"%s: sceIoLseek for setup failed[%#llx]\n",
			__func__, seekResult);
		result = seekResult;
		if (result >= 0)
			goto notify;
		error = result;
		goto write_error;
	}
	result = ksceIoWrite(context->file, buffer, size);
	if (result < 0) {
		SceDebugForDriver_1A3F2AA4(1, "%s: sceIoWrite failed[%#x]\n",
			__func__, result);
		error = result;
		goto write_error;
	}
notify:
	arguments = context->arguments;
	notification = context->notification;
	arguments->writtenDataSize += result;
	if (notification) {
		error = notification(arguments);
		if (error < 0)
			goto failed;
	}
	return result;
write_error:
	/* Sony inlines error_notification here, preserving the write error. */
	SceDebugForDriver_1A3F2AA4(1, "%s: sceIoWrite failed[%#x]\n",
		"error_notification", error);
	notification = context->notification;
	if (notification) {
		notificationResult = notification(context->arguments);
		if (notificationResult < 0)
			SceDebugForDriver_1A3F2AA4(1,
				"%s: return sceIoWrite error[%#x] instead of return value of notification[%#x]\n",
				"error_notification", error, notificationResult);
	}
failed:
	context->arguments->phase = 7;
	return error;
}
#endif

#define INITIALIZE_EXFAT_EXTENDED_SECTOR() do { \
	clearSize = sectorSize - 4; \
	memset(sector, 0, clearSize); \
	sector[clearSize] = 0; \
	sector[clearSize + 1] = 0; \
	sector[clearSize + 2] = 0x55; \
	sector[clearSize + 3] = 0xaa; \
	sectorSize = *bytesPerSector; \
	sector += sectorSize; \
} while (0)

int appmgrPrepareExfatBootRegion(AppmgrFatFormatArguments *arguments,
				const uint16_t *bytesPerSector)
{
	uint8_t *sector, *buffer;
	uint32_t sectorSize = *bytesPerSector;
	uint32_t clearSize, checksum = 0, coveredBytes, index;

	if (arguments->bufferSize < 12 * sectorSize)
		return (int)0x8001001c;
	sector = (uint8_t *)arguments->buffer + sectorSize;
	/* The eight extended sectors precede the OEM and reserved sectors. */
	INITIALIZE_EXFAT_EXTENDED_SECTOR();
	INITIALIZE_EXFAT_EXTENDED_SECTOR();
	INITIALIZE_EXFAT_EXTENDED_SECTOR();
	INITIALIZE_EXFAT_EXTENDED_SECTOR();
	INITIALIZE_EXFAT_EXTENDED_SECTOR();
	INITIALIZE_EXFAT_EXTENDED_SECTOR();
	INITIALIZE_EXFAT_EXTENDED_SECTOR();
	INITIALIZE_EXFAT_EXTENDED_SECTOR();
	memset(sector, 0, sectorSize);
	sectorSize = *bytesPerSector;
	sector += sectorSize;
	memset(sector, 0, sectorSize);
	sectorSize = *bytesPerSector;
	buffer = arguments->buffer;
	coveredBytes = 11 * sectorSize;
	for (index = 0; index < coveredBytes; index++) {
		if ((uint32_t)(index -
			offsetof(SceExfatfsBootSector, exfat.volumeFlags)) <= 1 ||
		    index == offsetof(SceExfatfsBootSector, exfat.percentInUse))
			continue;
		if (checksum & 1)
			checksum = (checksum >> 1) | 0x80000000;
		else
			checksum >>= 1;
		checksum += buffer[index];
	}
	sector += sectorSize;
	for (index = 0; index < (sectorSize >> 2); index++) {
		sector[0] = checksum;
		sector[1] = checksum >> 8;
		sector[2] = checksum >> 16;
		sector[3] = checksum >> 24;
		sector += 4;
	}
	return 0;
}

#undef INITIALIZE_EXFAT_EXTENDED_SECTOR

#if !FW_DEBUG
static inline unsigned int appmgrBuildFatClusterChain(
	AppmgrFatFormatContext *context, unsigned int cluster,
	unsigned int bufferOffset, uint64_t length, unsigned int bufferSize)
{
	AppmgrFatFormatArguments *arguments = context->arguments;
	uint8_t *entry = (uint8_t *)arguments->buffer + bufferOffset;
	uint8_t *end = entry + bufferSize;
	uint32_t clusterSize;
	int hasSpace = entry < end;

	if (arguments->clusterSize < length && hasSpace) {
		do {
			cluster++;
			entry[0] = cluster;
			entry[2] = cluster >> 16;
			entry[3] = cluster >> 24;
			entry[1] = cluster >> 8;
			entry += 4;
			clusterSize = arguments->clusterSize;
			hasSpace = entry < end;
			length -= clusterSize;
		} while (clusterSize < length && hasSpace);
	}
	if (length != 0 && hasSpace) {
		entry[0] = 0xff;
		entry[1] = 0xff;
		entry[2] = 0xff;
		if (context->type == 4)
			entry[3] = 0xff;
		else
			entry[3] = 0x0f;
		cluster++;
	}
	return cluster;
}
#endif

static inline void appmgrFillBadFatEntries(AppmgrFatFormatContext *context,
	unsigned int cluster, unsigned int bufferOffset, unsigned int count)
{
	uint8_t *entry = (uint8_t *)context->arguments->buffer + bufferOffset;
	uint8_t *end = entry + 4 * count;

	if (cluster == 0) {
		entry[0] = 0xf8;
		entry[1] = 0xff;
		entry[2] = 0xff;
		entry[3] = 0xff;
		entry[4] = 0xff;
		entry[5] = 0xff;
		entry[6] = 0xff;
		entry[7] = 0xff;
		entry += 8;
	}
	while (entry + 3 < end) {
		entry[0] = 0xf7;
		entry[1] = 0xff;
		entry[2] = 0xff;
		entry[3] = 0x0f;
		entry += 4;
	}
}

int _prepare_FAT32_area(AppmgrFatFormatContext *context)
{
	AppmgrFatFormatArguments *arguments = context->arguments;
	uint32_t fatSize = context->fatSectors * context->bytesPerSector;
	uint32_t fatOffset = context->reservedSectors * context->bytesPerSector;
	uint32_t cluster, entryOffset, clusterSize, copies;
	uint64_t length;
	uint8_t *fat;

	if (fatOffset + context->fatCount * fatSize > arguments->bufferSize)
		return (int)0x8001001c;
	copies = context->fatCount;
	while (copies != 0) {
		fat = (uint8_t *)context->arguments->buffer + fatOffset;
		memset(fat, 0, fatSize);
		fat[0] = 0xf8;
		fat[1] = 0xff;
		fat[2] = 0xff;
		fat[3] = 0xff;
		fat[4] = 0xff;
		fat[5] = 0xff;
		fat[6] = 0xff;
		fat[7] = 0xff;
		cluster = 2;
		entryOffset = 8;
		if (cluster < context->bitmapCluster1) {
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1,
				"%s: bad cluster[%#x] exists before bitmap1[%#x]\n",
				__func__, cluster, context->bitmapCluster1);
#endif
			appmgrFillBadFatEntries(context, cluster, fatOffset + entryOffset,
				context->bitmapCluster1 - cluster);
			cluster = context->bitmapCluster1;
			entryOffset = cluster * 4;
		}
		if (context->bitmapCluster1 != 0) {
			length = 0;
			if (context->type == 4) {
				clusterSize = context->arguments->clusterSize;
				length = (uint32_t)(((uint64_t)clusterSize - 1 +
					((context->clusterCount + 7) >> 3)) /
					clusterSize) * clusterSize;
			}
			cluster = appmgrBuildFatClusterChain(context, cluster,
				fatOffset + entryOffset, length, fatSize - entryOffset);
			entryOffset = cluster * 4;
		}
		if (cluster < context->bitmapCluster2) {
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1,
				"%s: bad cluster[%#x] exists before bitmap2[%#x]\n",
				__func__, cluster, context->bitmapCluster2);
#endif
			appmgrFillBadFatEntries(context, cluster, fatOffset + entryOffset,
				context->bitmapCluster2 - cluster);
			cluster = context->bitmapCluster2;
			entryOffset = cluster * 4;
		}
		if (context->bitmapCluster2 != 0) {
			length = 0;
			if (context->type == 4) {
				clusterSize = context->arguments->clusterSize;
				length = (uint32_t)(((uint64_t)clusterSize - 1 +
					((context->clusterCount + 7) >> 3)) /
					clusterSize) * clusterSize;
			}
			cluster = appmgrBuildFatClusterChain(context, cluster,
				fatOffset + entryOffset, length, fatSize - entryOffset);
			entryOffset = cluster * 4;
		}
		if (cluster < context->upcaseCluster) {
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1,
				"%s: bad cluster[%#x] exists before upcase_table[%#x]\n",
				__func__, cluster, context->upcaseCluster);
#endif
			appmgrFillBadFatEntries(context, cluster, fatOffset + entryOffset,
				context->upcaseCluster - cluster);
			cluster = context->bitmapCluster2;
			entryOffset = cluster * 4;
		}
		if (context->upcaseCluster != 0) {
#if FW_DEBUG
			length = appmgrGetFatUpcaseAllocationSize(context, 1);
#else
			length = 0;
			if (context->type == 4) {
				clusterSize = context->arguments->clusterSize;
				length = (uint32_t)((clusterSize + 5835) /
					clusterSize) * clusterSize;
			}
#endif
			cluster = appmgrBuildFatClusterChain(context, cluster,
				fatOffset + entryOffset, length, fatSize - entryOffset);
			entryOffset = cluster * 4;
		}
		if (cluster < context->rootCluster) {
			appmgrFillBadFatEntries(context, cluster, fatOffset + entryOffset,
				context->rootCluster - cluster);
			cluster = context->rootCluster;
			entryOffset = cluster * 4;
		}
		cluster = appmgrBuildFatClusterChain(context, cluster,
			fatOffset + entryOffset, arguments->clusterSize,
			fatSize - entryOffset);
#if FW_DEBUG
		if (context->firstUnusedCluster != cluster)
			SceDebugForDriver_1A3F2AA4(1,
				"%s: cluster[%#x] != expected[%#x]\n", __func__,
				cluster, context->firstUnusedCluster);
#endif
		copies--;
	}
	return copies;
}

static inline uint32_t _exfat_cluster_count(uint64_t volumeSize,
	uint32_t clusterSize, uint32_t sectorSize, uint64_t startLba,
	uint32_t reservedAlignment, uint32_t fatAlignment)
{
	uint64_t available, remaining, groupSize, groups, quotient;
	uint64_t fatCopiesSize = (uint32_t)(2 * fatAlignment);
	uint32_t entriesPerGroup = fatAlignment >> 2;
	uint32_t initialCount = entriesPerGroup - 2;
	uint32_t count;

	if (volumeSize < 0x100000) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1,
			"%s: volume_size(%llu) is too small for exFAT, reserved_alignment[%#x], fat_alignment[%#x], minimum_size(%llu)\n",
			__func__, volumeSize, reservedAlignment, fatAlignment,
			(uint64_t)0x100000);
#endif
		return 0;
	}
	available = volumeSize - fatCopiesSize + startLba * sectorSize -
		(((startLba + 24) * sectorSize + reservedAlignment - 1) /
		 reservedAlignment) * reservedAlignment;
	quotient = available / clusterSize;
	if (quotient < initialCount)
		return quotient;
	remaining = available - clusterSize * (uint64_t)initialCount;
	groupSize = entriesPerGroup * (uint64_t)clusterSize + fatCopiesSize;
	groups = remaining / groupSize;
	count = initialCount + (uint32_t)groups * entriesPerGroup;
	remaining -= groups * groupSize;
	if (fatCopiesSize < remaining)
		count += (remaining - fatCopiesSize) / clusterSize;
	return count;
}

static inline uint32_t _default_reserved(uint32_t type)
{
#if FW_DEBUG
	switch (type) {
	case 0:
	case 3:
		return 32;
	case 1:
	case 2:
		return 1;
	case 4:
		return 24;
	default:
		SceDebugForDriver_1A3F2AA4(1, "%s: unsupported type(%d)\n",
			__func__, type);
		return 0;
	}
#else
	return type <= 4 ? fatDefaultReservedSectors[type] : 0;
#endif
}

int appmgrConfigureFatFormat(AppmgrFatFormatContext *context, int index,
			     unsigned int requestedType, unsigned int clusterSize)
{
	AppmgrFatFormatArguments *arguments;
	uint32_t sectorSize = context->bytesPerSector;
	uint32_t alignment, count, reserved, rootEntries, type, fatBytes;
	uint32_t bitmapClusters, size, upcaseCluster;
	uint64_t startLba, volumeSize, rounded, fatSize;

	if (index == 1) {
		startLba = context->startLba;
		volumeSize = context->sectorCount * context->bytesPerSector;
	} else if (index == 2) {
		startLba = context->alternate.startLba;
		volumeSize = context->alternate.sectorCount * context->bytesPerSector;
	} else if (index == 0) {
		startLba = 0;
		volumeSize = context->arguments->volumeSize;
	} else {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: unsupported index(%d)\n",
			"_start_lba", index);
#endif
		startLba = UINT64_MAX;
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: unsupported index(%d)\n",
			"_volume_size", index);
#endif
		volumeSize = 0;
	}
	alignment = context->writeAlignment;
	if (sectorSize == 0 || sectorSize > 4096 || (sectorSize & 511) != 0) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: invalid sector_size(%u)\n",
			"_check_format_type", sectorSize);
#endif
		return (int)0x80010016;
	}
	if (clusterSize == 0) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: invalid cluster_size(%u)\n",
			"_check_format_type", clusterSize);
#endif
		return (int)0x80010016;
	}
	if (clusterSize % sectorSize != 0) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1,
			"%s: cluster_size(%u) isn't aligned for sector_size(%u)\n",
			"_check_format_type", clusterSize, sectorSize);
#endif
		return (int)0x80010016;
	}
	if (clusterSize > (requestedType <= 4 ?
		fatMaxClusterSizes[requestedType] : 0)) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: cluster_size(%u) is too large\n",
			"_check_format_type", clusterSize);
#endif
		return (int)0x80010016;
	}
	if (requestedType == 0) {
		count = _fat32_cluster_count(volumeSize, clusterSize, sectorSize,
			startLba, alignment, alignment);
		if (count != 0) {
			requestedType = 3;
		} else {
			count = _fat16_cluster_count(volumeSize, clusterSize, sectorSize,
				startLba, alignment, alignment);
			if (count != 0) {
				requestedType = 2;
			} else {
				count = _fat12_cluster_count(volumeSize, clusterSize,
					sectorSize, startLba, alignment, alignment);
				if (count == 0)
					return (int)0x80010016;
				requestedType = 1;
			}
		}
	} else {
		switch (requestedType) {
		case 1:
			count = _fat12_cluster_count(volumeSize, clusterSize, sectorSize,
				startLba, alignment, alignment);
			break;
		case 2:
			count = _fat16_cluster_count(volumeSize, clusterSize, sectorSize,
				startLba, alignment, alignment);
			break;
		case 3:
			count = _fat32_cluster_count(volumeSize, clusterSize, sectorSize,
				startLba, alignment, alignment);
			break;
		case 4:
			count = _exfat_cluster_count(volumeSize, clusterSize, sectorSize,
				startLba, alignment, alignment);
			break;
		default:
			return (int)0x80010016;
		}
		if (count == 0)
			return (int)0x80010016;
	}
#if FW_DEBUG
	alignment = context->writeAlignment;
#endif
	rootEntries = (uint32_t)(requestedType - 1) <= 1 ? 512 : 0;
	if (index == 1) {
#if FW_DEBUG
		sectorSize = context->bytesPerSector;
#endif
		startLba = context->startLba;
		context->type = requestedType;
		rounded = ((uint64_t)alignment - 1 +
			(startLba + _default_reserved(requestedType)) * sectorSize) /
			alignment;
		reserved = ((uint32_t)rounded * alignment -
			sectorSize * (uint32_t)startLba) / sectorSize;
		context->fatCount = 2;
		context->reservedSectors = reserved;
#if !FW_DEBUG
		context->legacyReservedSectors = reserved;
#endif
		context->rootEntryCount = rootEntries;
#if FW_DEBUG
		context->legacyReservedSectors = reserved;
#endif
		if (requestedType == 4) {
			context->upcaseTableSize = 5836;
		} else {
			context->upcaseTableSize = 0;
			context->bitmapCluster1 = 0;
			context->bitmapCluster2 = 0;
			context->upcaseCluster = 0;
			if (rootEntries != 0) {
				context->firstUnusedCluster = 2;
				context->rootCluster = 0;
			} else {
				context->rootCluster = 2;
				context->firstUnusedCluster = 3;
			}
		}
		arguments = context->arguments;
		arguments->clusterSize = clusterSize;
		type = context->type;
		switch (type) {
		case 1:
			fatBytes = count + 3 + (count >> 1) + (count & 1);
			break;
		case 2:
			fatBytes = 2 * (count + 2);
			break;
		case 3:
		case 4:
			fatBytes = 4 * (count + 2);
			break;
		default:
			fatBytes = 0;
		}
#if FW_DEBUG
		alignment = context->writeAlignment;
#endif
		context->clusterCount = count;
		fatSize = (((uint64_t)alignment - 1 + fatBytes) / alignment) *
			alignment;
		context->fatSectors = fatSize / sectorSize;
		if (type == 4) {
			size = arguments->clusterSize;
			bitmapClusters = (uint32_t)((uint32_t)
				(((uint64_t)size - 1 + ((count + 7) >> 3)) / size) *
				size) / size;
			context->bitmapCluster1 = 2;
			if (context->fatCount == 2) {
				context->bitmapCluster2 = bitmapClusters + 2;
				upcaseCluster = bitmapClusters + 2 + bitmapClusters;
			} else {
				context->bitmapCluster2 = 0;
				upcaseCluster = bitmapClusters + 2;
			}
			context->upcaseCluster = upcaseCluster;
			reserved = (size + context->upcaseTableSize - 1) / size;
#if !FW_DEBUG
			upcaseCluster = context->upcaseCluster;
#endif
			context->rootCluster = upcaseCluster + reserved;
			context->firstUnusedCluster = upcaseCluster + reserved + 1;
		}
	} else if (index == 2) {
#if FW_DEBUG
		sectorSize = context->bytesPerSector;
#endif
		startLba = context->alternate.startLba;
		context->alternate.type = requestedType;
		rounded = ((uint64_t)alignment - 1 +
			(startLba + _default_reserved(requestedType)) * sectorSize) /
			alignment;
		reserved = ((uint32_t)rounded * alignment -
			sectorSize * (uint32_t)startLba) / sectorSize;
		context->alternate.fatCount = 2;
		context->alternate.reservedSectors = reserved;
		context->alternate.rootEntryCount = rootEntries;
		if (requestedType == 4) {
			context->alternate.upcaseTableSize = 5836;
		} else {
			context->alternate.upcaseTableSize = 0;
			context->alternate.bitmapCluster1 = 0;
			context->alternate.bitmapCluster2 = 0;
			context->alternate.upcaseCluster = 0;
			if (rootEntries != 0) {
				context->alternate.rootCluster = 0;
				context->alternate.firstUnusedCluster = 2;
			} else {
				context->alternate.rootCluster = 2;
				context->alternate.firstUnusedCluster = 3;
			}
		}
		context->alternate.clusterSize = clusterSize;
		type = context->alternate.type;
		switch (type) {
		case 1:
			fatBytes = count + 3 + (count >> 1) + (count & 1);
			break;
		case 2:
			fatBytes = 2 * (count + 2);
			break;
		case 3:
		case 4:
			fatBytes = 4 * (count + 2);
			break;
		default:
			fatBytes = 0;
		}
		context->alternate.clusterCount = count;
		fatSize = (((uint64_t)alignment - 1 + fatBytes) / alignment) *
			alignment;
		context->alternate.fatSectors = fatSize / sectorSize;
		if (type == 4) {
			size = context->alternate.clusterSize;
			bitmapClusters = ((((uint64_t)size - 1 +
				((count + 7) >> 3)) / size) * size) / size;
			context->alternate.bitmapCluster1 = 2;
			if (context->alternate.fatCount == 2) {
				context->alternate.bitmapCluster2 = bitmapClusters + 2;
				upcaseCluster = bitmapClusters + 2 + bitmapClusters;
			} else {
				context->alternate.bitmapCluster2 = 0;
				upcaseCluster = bitmapClusters + 2;
			}
			context->alternate.upcaseCluster = upcaseCluster;
			reserved = (size + context->alternate.upcaseTableSize - 1) /
				size;
#if !FW_DEBUG
			upcaseCluster = context->alternate.upcaseCluster;
#endif
			context->alternate.rootCluster = upcaseCluster + reserved;
			context->alternate.firstUnusedCluster = upcaseCluster + reserved + 1;
		}
	} else {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: unsupported index(%d)\n",
			"_set_request_type", index);
		SceDebugForDriver_1A3F2AA4(1, "%s: unsupported index(%d)\n",
			"_set_cluster_size", index);
		SceDebugForDriver_1A3F2AA4(1, "%s: unsupported index(%d)\n",
			"_change_cluster_count", index);
#endif
	}
	return 0;
}

static inline void appmgrClearFatEntries(AppmgrFatFormatContext *context,
	unsigned int cluster, unsigned int bufferOffset, unsigned int size)
{
	uint8_t *buffer = (uint8_t *)context->arguments->buffer + bufferOffset;

	memset(buffer, 0, size);
	if (cluster == 0) {
		buffer[0] = 0xf8;
		buffer[1] = 0xff;
		buffer[2] = 0xff;
		buffer[3] = 0xff;
		buffer[4] = 0xff;
		buffer[5] = 0xff;
		buffer[6] = 0xff;
		buffer[7] = 0xff;
	}
}

#if !FW_DEBUG
static inline int device_fd_write(AppmgrFatFormatContext *context,
	const void *buffer, unsigned int size, uint64_t byteOffset)
{
	SceOff position;
	AppmgrFatFormatArguments *arguments;
	AppmgrFatFormatNotification notification;
	int result, error;

	position = ksceIoLseek(context->file, byteOffset, SCE_SEEK_SET);
	if (position >= 0)
		result = ksceIoWrite(context->file, buffer, size);
	else
		result = position;
	if (result < 0) {
		arguments = context->arguments;
		notification = context->notification;
		if (notification) {
			notification(arguments);
			arguments = context->arguments;
		}
		arguments->phase = 7;
		return result;
	}
	arguments = context->arguments;
	notification = context->notification;
	arguments->writtenDataSize += result;
	if (notification) {
		error = notification(arguments);
		if (error < 0) {
			context->arguments->phase = 7;
			return error;
		}
	}
	return result;
}
#endif

int initialize_FAT32(AppmgrFatFormatContext *context)
{
	AppmgrFatFormatArguments *arguments = context->arguments;
	AppmgrFatFormatArguments *initialArguments = context->arguments;
	unsigned int copies = context->fatCount;
	unsigned int sectorSize = context->bytesPerSector;
	unsigned int fatSize, remainingFat, bufferOffset, chunkSize;
	unsigned int windowStart, cluster, initialCluster, entryCount, previous;
	unsigned int target, gapEnd, size;
	uint64_t fatOffset, remainingLength, length, position;
	void *writeBuffer;
	int cleared, result;

	if (copies == 0)
		return 0x80010016;
	fatSize = context->fatSectors * sectorSize;
	fatOffset = (uint32_t)(context->reservedSectors * sectorSize);
	do {
		bufferOffset = sectorSize;
		chunkSize = arguments->bufferSize;
		remainingFat = fatSize;
		if (sectorSize >= chunkSize)
			bufferOffset = 0;
		else
			chunkSize -= sectorSize;
		writeBuffer = (uint8_t *)arguments->buffer + bufferOffset;
		windowStart = 0;
		remainingLength = 0;
		cleared = 0;
		for (;;) {
			cluster = windowStart ? windowStart : 2;
			initialCluster = cluster;
			if (chunkSize >= remainingFat)
				chunkSize = remainingFat;
			entryCount = chunkSize >> 2;
			target = context->bitmapCluster1;
			if (windowStart < target) {
				cluster = windowStart + entryCount;
				if (target <= cluster) {
					appmgrClearFatEntries(context, windowStart,
						bufferOffset, chunkSize);
					appmgrFillBadFatEntries(context, windowStart,
						bufferOffset,
						context->bitmapCluster1 - windowStart);
					cluster = context->bitmapCluster1;
				} else {
					appmgrFillBadFatEntries(context, windowStart,
						bufferOffset, entryCount);
				}
			}
			if (cluster == initialCluster) {
				if (!cleared)
					appmgrClearFatEntries(context, windowStart,
						bufferOffset, chunkSize);
				cleared = 1;
			}
			if (cluster == context->bitmapCluster1) {
#if FW_DEBUG
				if (remainingLength != 0)
					SceDebugForDriver_1A3F2AA4(1,
						"%s: unexpected file_size(%llu)\n",
						__func__, remainingLength);
#endif
				if (context->type == 4) {
					size = context->arguments->clusterSize;
					remainingLength = (uint32_t)((
						(uint64_t)size - 1 +
						((context->clusterCount + 7) >> 3)) /
						size) * size;
				} else {
					remainingLength = 0;
				}
				if (cluster - windowStart < entryCount) {
					cleared = 0;
					previous = cluster;
					cluster = appmgrBuildFatClusterChain(context, cluster,
						4 * (cluster - windowStart) + bufferOffset,
						remainingLength,
						chunkSize - 4 * (cluster - windowStart));
					remainingLength -= (uint32_t)(
						initialArguments->clusterSize * (cluster - previous));
				}
			}
			target = context->bitmapCluster2;
			if (target != 0 && cluster - windowStart < entryCount) {
				if (cluster < target) {
					if (remainingLength != 0) {
						previous = cluster;
						cluster = appmgrBuildFatClusterChain(context, cluster,
							4 * (cluster - windowStart) + bufferOffset,
							remainingLength,
							chunkSize - 4 * (cluster - windowStart));
						remainingLength -= (uint32_t)(initialArguments->clusterSize *
							(cluster - previous));
						target = context->bitmapCluster2;
					}
					if (cluster < target && cluster - windowStart < entryCount) {
#if FW_DEBUG
						if (remainingLength != 0)
							SceDebugForDriver_1A3F2AA4(1,
								"%s: unexpected file_size(%llu)\n",
								__func__, remainingLength);
						target = context->bitmapCluster2;
#endif
						gapEnd = windowStart + entryCount;
						if (gapEnd >= target)
							gapEnd = target;
						appmgrFillBadFatEntries(context, cluster,
							4 * (cluster - windowStart) + bufferOffset,
							gapEnd - cluster);
						if (windowStart + entryCount < target)
							cluster = windowStart + entryCount;
						else
							cluster = context->bitmapCluster2;
					}
				}
				if (cluster == context->bitmapCluster2) {
#if FW_DEBUG
					if (remainingLength != 0)
						SceDebugForDriver_1A3F2AA4(1,
							"%s: unexpected file_size(%llu)\n",
							__func__, remainingLength);
#endif
					if (cluster - windowStart < entryCount) {
						if (context->type == 4) {
							size = context->arguments->clusterSize;
							length = (uint32_t)(((uint64_t)size - 1 +
								((context->clusterCount + 7) >> 3)) / size) * size;
						} else {
							length = 0;
						}
						previous = cluster;
						cluster = appmgrBuildFatClusterChain(context, cluster,
							4 * (cluster - windowStart) + bufferOffset,
							length, chunkSize - 4 * (cluster - windowStart));
						remainingLength = length - (uint32_t)(
							initialArguments->clusterSize * (cluster - previous));
					}
				}
				cleared = 0;
			}
			target = context->upcaseCluster;
			if (target != 0 && cluster - windowStart < entryCount) {
				if (cluster < target) {
					if (remainingLength != 0) {
						previous = cluster;
						cluster = appmgrBuildFatClusterChain(context, cluster,
							4 * (cluster - windowStart) + bufferOffset,
							remainingLength,
							chunkSize - 4 * (cluster - windowStart));
						remainingLength -= (uint32_t)(initialArguments->clusterSize *
							(cluster - previous));
						target = context->upcaseCluster;
					}
					if (cluster < target && cluster - windowStart < entryCount) {
#if FW_DEBUG
						if (remainingLength != 0)
							SceDebugForDriver_1A3F2AA4(1,
								"%s: unexpected file_size(%llu)\n",
								__func__, remainingLength);
						target = context->upcaseCluster;
#endif
						gapEnd = windowStart + entryCount;
						if (gapEnd >= target)
							gapEnd = target;
						appmgrFillBadFatEntries(context, cluster,
							4 * (cluster - windowStart) + bufferOffset,
							gapEnd - cluster);
						if (windowStart + entryCount < target)
							cluster = windowStart + entryCount;
						else
							cluster = context->upcaseCluster;
					}
				}
				if (cluster == context->upcaseCluster &&
				    cluster - windowStart < entryCount) {
#if FW_DEBUG
					length = appmgrGetFatUpcaseAllocationSize(context, 1);
#else
					if (context->type == 4) {
						size = context->arguments->clusterSize;
						length = ((size + 5835) / size) * size;
					} else {
						length = 0;
					}
#endif
					previous = cluster;
					cluster = appmgrBuildFatClusterChain(context, cluster,
						4 * (cluster - windowStart) + bufferOffset,
						length, chunkSize - 4 * (cluster - windowStart));
					remainingLength = length - (uint32_t)(
						initialArguments->clusterSize * (cluster - previous));
				}
				cleared = 0;
			}
			if (cluster < context->rootCluster &&
			    cluster - windowStart < entryCount && remainingLength != 0) {
				cleared = 0;
				previous = cluster;
				cluster = appmgrBuildFatClusterChain(context, cluster,
					4 * (cluster - windowStart) + bufferOffset,
					remainingLength,
					chunkSize - 4 * (cluster - windowStart));
				remainingLength -= (uint32_t)(initialArguments->clusterSize *
					(cluster - previous));
			}
			if (cluster == initialCluster && !cleared) {
				appmgrClearFatEntries(context, windowStart, bufferOffset,
					chunkSize);
				if (windowStart != 0)
					cleared = cluster == windowStart;
			}
			target = context->rootCluster;
			if (cluster < target && cluster - windowStart < entryCount) {
				gapEnd = windowStart + entryCount;
				if (target <= gapEnd)
					gapEnd = target;
				appmgrFillBadFatEntries(context, cluster,
					4 * (cluster - windowStart) + bufferOffset,
					gapEnd - cluster);
				if (target > windowStart + entryCount)
					cluster = windowStart + entryCount;
				else
					cluster = context->rootCluster;
				cleared = 0;
			}
			if (cluster == context->rootCluster) {
				if (cluster - windowStart >= entryCount)
					goto write_chunk;
				cleared = 0;
				cluster = appmgrBuildFatClusterChain(context, cluster,
					4 * (cluster - windowStart) + bufferOffset,
					initialArguments->clusterSize,
					chunkSize - 4 * (cluster - windowStart));
			}
			if (cluster - windowStart < entryCount && !cleared) {
				appmgrClearFatEntries(context, cluster,
					4 * (cluster - windowStart) + bufferOffset,
					chunkSize - 4 * (cluster - windowStart));
				cleared = cluster == windowStart;
			}
write_chunk:
			position = fatOffset + (uint32_t)(4 * windowStart);
			if (context->currentLba != context->startLba)
				position += (context->startLba - context->currentLba) *
					context->bytesPerSector;
			result = device_fd_write(context, writeBuffer, chunkSize, position);
			if (result < 0)
				return result;
			remainingFat -= chunkSize;
			if (remainingFat == 0)
				break;
			windowStart += entryCount;
		}
		sectorSize = context->bytesPerSector;
		if (--copies != 0) {
			fatSize = context->fatSectors * sectorSize;
			arguments = context->arguments;
			fatOffset += fatSize;
		}
	} while (copies != 0);
	return result;
}

static inline uint32_t appmgrUpdateExfatBootChecksum(uint32_t checksum,
	const uint8_t *buffer, unsigned int size)
{
	const uint8_t *end = buffer + size;

	while (buffer != end) {
		if (checksum & 1)
			checksum = (checksum >> 1) | 0x80000000;
		else
			checksum >>= 1;
		checksum += *buffer++;
	}
	return checksum;
}

static inline int appmgrWriteExfatBootCopies(AppmgrFatFormatContext *context,
	AppmgrFatFormatArguments *arguments, const void *buffer, unsigned int size,
	unsigned int offset, uint64_t backupDistance)
{
	uint64_t position = offset;
	int result;

	if (context->currentLba != context->startLba)
		position += (context->startLba - context->currentLba) *
			context->bytesPerSector;
	result = device_fd_write(context, buffer, size, position);
	size = context->bytesPerSector;
	if (size == result) {
		position = offset + backupDistance;
		if (context->currentLba != context->startLba)
			position += (context->startLba - context->currentLba) *
				context->bytesPerSector;
		result = device_fd_write(context, arguments->buffer, size, position);
	}
	return result;
}

int appmgrWriteExfatExtendedBootRegion(AppmgrFatFormatContext *context,
	unsigned int checksum)
{
	unsigned int sectorSize = context->bytesPerSector;
	AppmgrFatFormatArguments *arguments = context->arguments;
	uint8_t *buffer = arguments->buffer;
	unsigned int trailerOffset = sectorSize - 4;
	uint64_t backupDistance = 12 * sectorSize;
	unsigned int offset = 0, count = 8, index;
	int result;

	memset(buffer, 0, trailerOffset);
	buffer[trailerOffset] = 0;
	buffer[trailerOffset + 1] = 0;
	buffer[trailerOffset + 2] = 0x55;
	buffer[trailerOffset + 3] = 0xaa;
	do {
		sectorSize = context->bytesPerSector;
		offset += sectorSize;
		buffer = arguments->buffer;
		checksum = appmgrUpdateExfatBootChecksum(checksum, buffer, sectorSize);
		result = appmgrWriteExfatBootCopies(context, arguments, buffer,
			sectorSize, offset, backupDistance);
		if (result < 0)
			return result;
	} while (--count != 0);
	sectorSize = context->bytesPerSector;
	offset += sectorSize;
	memset(arguments->buffer, 0, sectorSize);
	sectorSize = context->bytesPerSector;
	buffer = arguments->buffer;
	checksum = appmgrUpdateExfatBootChecksum(checksum, buffer, sectorSize);
	result = appmgrWriteExfatBootCopies(context, arguments, buffer,
		sectorSize, offset, backupDistance);
	if (result < 0)
		return result;
	sectorSize = context->bytesPerSector;
	memset(arguments->buffer, 0, sectorSize);
	offset += sectorSize;
	sectorSize = context->bytesPerSector;
	buffer = arguments->buffer;
	checksum = appmgrUpdateExfatBootChecksum(checksum, buffer, sectorSize);
	result = appmgrWriteExfatBootCopies(context, arguments, buffer,
		sectorSize, offset, backupDistance);
	if (result < 0)
		return result;
	sectorSize = context->bytesPerSector;
	offset += sectorSize;
	buffer = arguments->buffer;
	count = sectorSize >> 2;
	for (index = 0; index < count; index++) {
		buffer[0] = checksum;
		buffer[1] = checksum >> 8;
		buffer[2] = checksum >> 16;
		buffer[3] = checksum >> 24;
		buffer += 4;
	}
	if (count != 0) {
		sectorSize = context->bytesPerSector;
		buffer = arguments->buffer;
	}
	return appmgrWriteExfatBootCopies(context, arguments, buffer,
		sectorSize, offset, backupDistance);
}

unsigned int appmgrGetFatInitialAllocationSize(AppmgrFatFormatContext *context,
	int index)
{
	unsigned int clusterSize, type, bitmapSize = 0;
	unsigned int count, size, copies, upcaseSize = 0;
	uint64_t bitmapClusters;

#if FW_DEBUG
	clusterSize = _cluster_size(context, index);
	type = _request_type(context, index);
#else
	if (index == 1) {
		clusterSize = context->arguments->clusterSize;
		type = context->type;
	} else if (index == 2) {
		clusterSize = context->alternate.clusterSize;
		type = context->alternate.type;
	} else {
		return 0;
	}
#endif
	if (type == 3)
		return clusterSize;
	if (type != 4)
		return 0;
	if (index == 1) {
		if (context->type == 4) {
			copies = context->fatCount;
			size = context->arguments->clusterSize;
			count = context->clusterCount;
			bitmapClusters = ((uint64_t)size - 1 + ((count + 7) >> 3)) /
				size;
			bitmapSize = (uint32_t)bitmapClusters * (size * copies);
		}
	} else if (index == 2) {
		if (context->alternate.type == 4) {
			copies = context->alternate.fatCount;
			size = context->alternate.clusterSize;
			count = context->alternate.clusterCount;
			bitmapClusters = ((uint64_t)size - 1 + ((count + 7) >> 3)) /
				size;
			bitmapSize = (uint32_t)bitmapClusters * (size * copies);
		}
	}
#if FW_DEBUG
	upcaseSize = appmgrGetFatUpcaseAllocationSize(context, index);
#else
	if (index == 1) {
		if (context->type == 4) {
			size = context->arguments->clusterSize;
			upcaseSize = ((size + 5835) / size) * size;
		}
	} else if (index == 2) {
		if (context->alternate.type == 4) {
			size = context->alternate.clusterSize;
			upcaseSize = ((size + 5835) / size) * size;
		}
	}
#endif
	return clusterSize + upcaseSize + bitmapSize;
}

static inline int appmgrFatPathHasPrefix(const char *path, const char *prefix)
{
	while (*prefix != 0) {
		if (*path++ != *prefix++)
			return 0;
	}
	return 1;
}

static inline int _check_buffer_alignment(AppmgrFatFormatArguments *arguments,
	unsigned int sectorSize)
{
	void *buffer = arguments->buffer;
	unsigned int bufferSize, remainder;

	if (buffer == NULL)
		return (int)0x80010016;
#if FW_DEBUG
	if ((uintptr_t)buffer % sectorSize != 0)
		SceDebugForDriver_1A3F2AA4(1,
			"%s: unaligned buffer[%p] takes long time for device access\n",
			__func__, buffer);
#endif
	bufferSize = arguments->bufferSize;
	if (sectorSize > bufferSize) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1,
			"%s: buffer_size(%u) < sector_size(%u)\n",
			__func__, bufferSize, sectorSize);
#endif
		return (int)0x80010016;
	}
	remainder = bufferSize % sectorSize;
	if (remainder != 0) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1,
			"%s: reduce (%u) from buffer_size(%u) for sector alignment\n",
			__func__, remainder, bufferSize);
		arguments->bufferSize -= remainder;
#else
		arguments->bufferSize = bufferSize - remainder;
#endif
	}
	return 0;
}

static inline void _set_start_lba(AppmgrFatFormatContext *context, int index,
	uint64_t startLba)
{
	if (index == 1)
		context->startLba = startLba;
	else if (index == 2)
		context->alternate.startLba = startLba;
#if FW_DEBUG
	else
		SceDebugForDriver_1A3F2AA4(1, "%s: unsupported index(%d)\n", __func__, index);
#endif
}

static inline void _set_volume_size(AppmgrFatFormatContext *context, int index,
	uint64_t volumeSize)
{
	if (index == 1)
		context->sectorCount = volumeSize / context->bytesPerSector;
	else if (index == 2)
		context->alternate.sectorCount = volumeSize / context->bytesPerSector;
#if FW_DEBUG
	else
		SceDebugForDriver_1A3F2AA4(1, "%s: unsupported index(%d)\n", __func__, index);
#endif
}

static inline int _prepare_partition_table(AppmgrFatFormatContext *context)
{
	AppmgrFatFormatArguments *arguments = context->arguments;
	SceMbr *mbr = arguments->buffer;
	unsigned int systemId, type = arguments->type, clusterSize;
	int result, index, task;
	uint64_t partitionSize, startLba;

#if FW_DEBUG
	switch (type) {
	case 1: systemId = 1; break;
	case 2: systemId = 6; break;
	case 3: systemId = 11; break;
	case 4: systemId = 7; break;
	default:
		SceDebugForDriver_1A3F2AA4(1, "%s: unsupported request_type(%d)\n",
			__func__, type);
		return (int)0x80010016;
	}
#else
	if (type - 1 > 3)
		return (int)0x80010016;
	systemId = fatPartitionSystemIds[type - 1];
#endif
	result = _fs_util_prepare_mbr(arguments->deviceName, systemId, mbr,
		512, arguments->volumeSize);
	if (result != 0)
		return result;
	partitionSize = appmgrMbrGetPartitionSize(arguments->buffer, 0);
	if (partitionSize != 0) {
		index = 0;
		task = 1;
		do {
			clusterSize = appmgrMbrGetClusterSize(arguments->buffer, index);
			if (clusterSize != 0) {
				startLba = appmgrMbrGetPartitionOffset(arguments->buffer, index);
				_set_start_lba(context, task, startLba);
				_set_volume_size(context, task,
					partitionSize * context->bytesPerSector);
#if FW_DEBUG
				if (clusterSize != _cluster_size(context, task))
					SceDebugForDriver_1A3F2AA4(1,
						"%s: change cluster_size in arguments[%#x] -> [%#x] in partition_conf.h, partition_index(%d), format_task(%d)\n",
						__func__, _cluster_size(context, task),
						clusterSize, index, task);
#endif
				type = appmgrMbrGetFormatType(arguments->buffer, index);
				result = appmgrConfigureFatFormat(context, task++, type, clusterSize);
			}
			partitionSize = appmgrMbrGetPartitionSize(arguments->buffer, ++index);
		} while (partitionSize != 0);
	} else {
		for (index = 0; index < 4; index++) {
			partitionSize = appmgrMbrGetFdiskSize(arguments->buffer, index);
			if (partitionSize == 0)
				break;
			startLba = appmgrMbrGetFdiskOffset(arguments->buffer, index);
			_set_start_lba(context, index + 1, startLba);
			_set_volume_size(context, index + 1,
				partitionSize * context->bytesPerSector);
			result = appmgrConfigureFatFormat(context, index + 1,
				arguments->type, arguments->clusterSize);
		}
	}
	return result;
}

int _check_arguments(AppmgrFatFormatArguments *arguments,
	AppmgrFatFormatContext *context, int needBuffer)
{
	SceSdstorPartitionInfo info;
	const char *device, *end, *suffix;
	uint64_t endPosition, startPosition, deviceSize, volumeSize;
	unsigned int sectorSize, writeAlignment;
	SceUID file;
	int result, closeResult, wholeDevice = 0;

#if FW_DEBUG
	if (arguments->phase != 0)
		SceDebugForDriver_1A3F2AA4(1, "%s: initialize phase(%d)\n",
			__func__, arguments->phase);
#endif
	arguments->phase = 1;
	memset(context, 0, sizeof(*context));
	context->arguments = arguments;
	context->notification = appmgrFatFormatDefaultNotification;
	context->bytesPerSector = 512;
	context->file = -1;
	context->writeAlignment = arguments->clusterSize;
	if (arguments->writtenDataSize != 0) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: initialize written_data_size[%#x]\n",
			__func__, arguments->writtenDataSize);
#endif
		arguments->writtenDataSize = 0;
	}
	file = ksceIoOpen(arguments->deviceName, 0x04000003, 0);
	if (file < 0) {
		result = file;
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: sceIoOpen for (%s) failed[%#x]\n",
			__func__, arguments->deviceName, result);
#endif
		goto failed;
	}
	endPosition = ksceIoLseek(file, 0, SCE_SEEK_END);
	startPosition = ksceIoLseek(file, 0, SCE_SEEK_SET);
	context->file = file;
	deviceSize = endPosition - startPosition;
	result = _fs_util_get_device_info(arguments->deviceName, &info, sizeof(info));
	if (result == 0) {
#if FW_DEBUG
		if (context->bytesPerSector * (uint64_t)info.sectorCount != deviceSize)
			SceDebugForDriver_1A3F2AA4(1,
				"%s: volumeSize[%#llx] != sectorCount[%#x] * sector_size[%#x]\n",
				__func__, deviceSize, info.sectorCount, context->bytesPerSector);
#endif
		context->currentLba = info.startSector;
		context->startLba = info.startSector;
		device = arguments->deviceName;
		if (device != NULL && appmgrFatPathHasPrefix(device, "sdstor0:int-")) {
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1,
				"%s: set hidden_sectors[%#x] and use write_alignment[%#x] instead of blockSize[%#x], device_name(%s)\n",
				__func__, info.startSector, context->writeAlignment,
				info.eraseBlockCount, device);
#endif
		} else {
			context->writeAlignment = info.eraseBlockCount;
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1,
				"%s: set hidden_sectors[%#x] and write_alignment[%#x], device_name(%s)\n",
				__func__, info.startSector, info.eraseBlockCount, device);
#endif
		}
	} else {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1,
			"%s: _fs_util_get_device_info for (%s) failed[%#x]\n",
			__func__, arguments->deviceName, result);
#endif
	}
	sectorSize = context->bytesPerSector;
	volumeSize = arguments->volumeSize;
	if (volumeSize % sectorSize != 0) {
		result = (int)0x80010016;
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1,
			"%s: request volume_size[%#llx] isn't aligned with sector_size[%#x]\n",
			__func__, volumeSize, sectorSize);
#endif
		goto close_failed;
	}
	if (deviceSize < volumeSize) {
		result = (int)0x8001001c;
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: request volume_size[%#llx] > device_size[%#llx]\n",
			__func__, volumeSize, deviceSize);
#endif
		goto close_failed;
	}
	device = arguments->deviceName;
	end = device;
	while (*end != 0)
		end++;
	if (end - device > 6) {
		suffix = end - 7;
		if (suffix != NULL && appmgrFatPathHasPrefix(suffix, "-entire") &&
		    *device == 's') {
			if (appmgrFatPathHasPrefix(device, "sdstor0:gcd-") ||
			    appmgrFatPathHasPrefix(device, "sdstor0:xmc-")) {
				wholeDevice = 1;
				result = 0;
			} else if (appmgrFatPathHasPrefix(device, "sdstor0:ext-") &&
				   deviceSize != volumeSize) {
				wholeDevice = 1;
				result = 0;
				if (arguments->type == 0) {
					writeAlignment = context->writeAlignment;
					if (volumeSize == 0)
						volumeSize = deviceSize - writeAlignment;
					arguments->type = appmgrSelectFatFormat(volumeSize,
						arguments->clusterSize, sectorSize,
						writeAlignment / sectorSize, writeAlignment);
					if (arguments->type == 0)
						result = (int)0x80010016;
				}
			}
		}
	}
	if (wholeDevice) {
		if (arguments->initialImageSize != 0 && arguments->initialImageSize != 512) {
			result = (int)0x80010016;
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1, "%s: invalid initial_image_size[%#x]\n",
				__func__, arguments->initialImageSize);
#endif
			goto close_failed;
		}
		if (result != 0)
			goto close_failed;
		result = _check_buffer_alignment(arguments, context->bytesPerSector);
		if (result != 0)
			goto close_failed;
		arguments->phase = 8;
		result = _prepare_partition_table(context);
		if (arguments->volumeSize == 0) {
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1,
				"%s: whole device size[%#llx], set partition size[%#llx]\n",
				__func__, deviceSize,
				context->sectorCount * context->bytesPerSector);
#endif
			arguments->volumeSize = context->sectorCount * context->bytesPerSector;
		}
	} else {
		if (arguments->initialImageSize != 0) {
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1, "%s: invalidate initial_image_size[%#x]\n",
				__func__, arguments->initialImageSize);
			volumeSize = arguments->volumeSize;
#endif
			arguments->initialImageSize = 0;
		}
		if (volumeSize == 0) {
			volumeSize = deviceSize;
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1, "%s: use whole device, size[%#llx]\n",
				__func__, deviceSize);
#endif
			arguments->volumeSize = deviceSize;
		}
		if (needBuffer != 0) {
			result = _check_buffer_alignment(arguments, context->bytesPerSector);
			if (result != 0)
				goto close_failed;
		}
		context->sectorCount = arguments->volumeSize / context->bytesPerSector;
		result = appmgrConfigureFatFormat(context, 1, arguments->type,
			arguments->clusterSize);
	}
	if (result != 0) {
		if (result < 0)
			goto close_failed;
		goto failed;
	}
	if (arguments->type != context->type) {
#if FW_DEBUG
		if (arguments->type != 0)
			SceDebugForDriver_1A3F2AA4(1, "%s: format_type(%d) != arguments->type(%d)\n",
				__func__, context->type, arguments->type);
		else
			SceDebugForDriver_1A3F2AA4(1, "%s: set format_type(%d)\n",
				__func__, context->type);
#endif
		arguments->type = context->type;
	}
	return result;
close_failed:
	closeResult = ksceIoClose(context->file);
	if (closeResult >= 0)
		context->file = -1;
#if FW_DEBUG
	else
		SceDebugForDriver_1A3F2AA4(1, "%s: sceIoClose failed[%#x]\n", __func__, closeResult);
#endif
failed:
	arguments->phase = 7;
	return result;
}

static inline void appmgrFatStoreLe32(void *destination, uint32_t value)
{
	uint8_t *buffer = destination;

	buffer[0] = value;
	buffer[1] = value >> 8;
	buffer[2] = value >> 16;
	buffer[3] = value >> 24;
}

static inline void appmgrFatStoreLe64(void *destination, uint64_t value)
{
	uint8_t *buffer = destination;

	buffer[0] = value;
	value >>= 8;
	buffer[1] = value;
	value >>= 8;
	buffer[2] = value;
	value >>= 8;
	buffer[3] = value;
	value >>= 8;
	buffer[4] = value;
	value >>= 8;
	buffer[5] = value;
	value >>= 8;
	buffer[6] = value;
	value >>= 8;
	buffer[7] = value;
}

unsigned int appmgrSetExfatBootGeometry(SceExfatfsBootSector *sector,
	AppmgrFatFormatContext *context)
{
	uint32_t startLba = context->startLba;
	AppmgrFatFormatArguments *arguments = context->arguments;
	uint64_t volumeLength;
	uint32_t value, sectorSize, clusterSize;
	uint8_t sectorShift = 9, clusterShift = 0;
	unsigned int copies;

	appmgrFatStoreLe64(&sector->exfat.partitionOffset, startLba);
	volumeLength = arguments->volumeSize / context->bytesPerSector;
	appmgrFatStoreLe64(&sector->exfat.volumeLength, volumeLength);
	appmgrFatStoreLe32(&sector->exfat.fatOffset, context->reservedSectors);
	appmgrFatStoreLe32(&sector->exfat.fatLength, context->fatSectors);
	appmgrFatStoreLe32(&sector->exfat.clusterHeapOffset,
		context->reservedSectors + context->fatSectors * context->fatCount);
	appmgrFatStoreLe32(&sector->exfat.clusterCount, context->clusterCount);
	appmgrFatStoreLe32(&sector->exfat.rootCluster, context->rootCluster);
	value = arguments->serial;
	sector->exfat.bytesPerSectorShift = 9;
	appmgrFatStoreLe32(sector->exfat.volumeSerial, value);
	sectorSize = context->bytesPerSector;
	if (sectorSize > 512) {
		sectorSize >>= 9;
		if (sectorSize != 1) {
			do {
				sectorSize >>= 1;
				sectorShift++;
			} while (sectorSize != 1);
			sector->exfat.bytesPerSectorShift = sectorShift;
		}
	}
	sector->exfat.sectorsPerClusterShift = 0;
	sectorSize = context->bytesPerSector;
	clusterSize = arguments->clusterSize;
	if (clusterSize > sectorSize) {
		value = clusterSize / sectorSize;
		if (value > 1) {
			do {
				value >>= 1;
				clusterShift++;
			} while (value > 1);
			sector->exfat.sectorsPerClusterShift = clusterShift;
		}
	}
	copies = context->fatCount;
	sector->exfat.numberOfFats = copies;
	return copies;
}

void *appmgrInitializeExfatBootSector(SceExfatfsBootSector *sector,
	AppmgrFatFormatContext *context)
{
	void *result;

	sector->exfat.jump[0] = 0xeb;
	sector->exfat.jump[1] = 0x76;
	sector->exfat.jump[2] = 0x90;
	sector->exfat.fileSystemName[0] = 'E';
	sector->exfat.fileSystemName[1] = 'X';
	sector->exfat.fileSystemName[2] = 'F';
	sector->exfat.fileSystemName[3] = 'A';
	sector->exfat.fileSystemName[4] = 'T';
	sector->exfat.fileSystemName[5] = ' ';
	sector->exfat.fileSystemName[6] = ' ';
	sector->exfat.fileSystemName[7] = ' ';
	memset(sector->exfat.mustBeZero, 0, sizeof(sector->exfat.mustBeZero));
	if (context != NULL) {
		appmgrSetExfatBootGeometry(sector, context);
	} else {
		appmgrFatStoreLe64(&sector->exfat.partitionOffset, 0);
		appmgrFatStoreLe64(&sector->exfat.volumeLength, exfatDefaultVolumeLength);
		appmgrFatStoreLe32(&sector->exfat.fatOffset, exfatDefaultFatOffset);
		appmgrFatStoreLe32(&sector->exfat.fatLength, exfatDefaultFatLength);
		appmgrFatStoreLe32(&sector->exfat.clusterHeapOffset, exfatDefaultHeapOffset);
		appmgrFatStoreLe32(&sector->exfat.clusterCount, exfatDefaultClusterCount);
		appmgrFatStoreLe32(&sector->exfat.rootCluster, exfatDefaultRootCluster);
		appmgrFatStoreLe32(sector->exfat.volumeSerial, 0);
		sector->exfat.bytesPerSectorShift = 9;
		sector->exfat.sectorsPerClusterShift = exfatDefaultClusterShift;
		sector->exfat.numberOfFats = 1;
	}
	sector->exfat.driveSelect = 0x80;
	sector->exfat.revision = 0x0100;
	sector->exfat.volumeFlags = 0;
	sector->exfat.reserved[0] = 0;
	sector->exfat.reserved[1] = 0;
	sector->exfat.reserved[2] = 0;
	sector->exfat.reserved[3] = 0;
	sector->exfat.reserved[4] = 0;
	sector->exfat.reserved[5] = 0;
	sector->exfat.reserved[6] = 0;
	sector->exfat.percentInUse = 0xff;
	result = memset(sector->exfat.bootCode, 0xf4, sizeof(sector->exfat.bootCode));
	sector->exfat.signature = 0xaa55;
	return result;
}

static inline uint64_t appmgrFatClusterDataOffset(AppmgrFatFormatContext *context)
{
	uint64_t alignment = context->writeAlignment;
	uint64_t copies = (uint64_t)context->fatCount * (uint32_t)alignment;
	uint32_t entries = 4 * (context->clusterCount + 2);

	return copies * ((alignment - 1 + entries) / context->writeAlignment) +
		(uint32_t)(context->reservedSectors * context->bytesPerSector);
}

static inline uint64_t appmgrFatDeviceOffset(AppmgrFatFormatContext *context,
	uint64_t offset)
{
	uint64_t current = context->currentLba;
	uint32_t start = context->startLba;

	if (current != start)
		offset += (start - current) * context->bytesPerSector;
	return offset;
}

static inline unsigned int appmgrFatUpcaseChecksum(void)
{
	const unsigned char *cursor = (const unsigned char *)exfatDefaultUpcaseTable;
	const unsigned char *end = cursor + sizeof(exfatDefaultUpcaseTable);
	unsigned int checksum = 0;

	do {
		checksum = ((checksum >> 1) | (checksum << 31)) + *cursor++;
	} while (cursor != end);
	return checksum;
}

static inline unsigned int appmgrFatBitmapAllocationSize(
	AppmgrFatFormatContext *context)
{
	uint32_t clusterSize;
	uint64_t size;

	if (context->type != 4)
		return 0;
	clusterSize = context->arguments->clusterSize;
	size = clusterSize - 1ULL + ((context->clusterCount + 7) >> 3);
	return (uint32_t)(size / clusterSize) * clusterSize;
}

static inline void appmgrFatInitializeBitmap(unsigned char *buffer,
	uint32_t firstUnused)
{
	unsigned int cluster = 2;

	while (firstUnused > cluster + 7) {
		cluster += 8;
		*buffer++ = 0xff;
	}
	while (firstUnused > cluster++)
		*buffer = (*buffer << 1) | 1;
}

static inline void appmgrFatPrepareBitmapEntry(
	SceExfatfsAllocationBitmapEntry *entry, unsigned int flags,
	uint32_t cluster, uint32_t clusterCount)
{
	entry->entryType = 0x81;
	entry->bitmapFlags = flags;
	memset(entry->reserved, 0, sizeof(entry->reserved));
	appmgrFatStoreLe32(entry->firstCluster, cluster);
	appmgrFatStoreLe64(entry->dataLength, (clusterCount + 7) >> 3);
}

static inline void appmgrFatPrepareUpcaseEntry(
	SceExfatfsUpcaseTableEntry *entry, unsigned int checksum, uint32_t cluster)
{
	entry->entryType = 0x82;
	memset(entry->reserved, 0, sizeof(entry->reserved));
	appmgrFatStoreLe32(entry->tableChecksum, checksum);
	memset(entry->reservedTable, 0, sizeof(entry->reservedTable));
	appmgrFatStoreLe32(entry->firstCluster, cluster);
	appmgrFatStoreLe64(entry->dataLength, sizeof(exfatDefaultUpcaseTable));
}

static inline void appmgrFatPrepareLabelEntry(SceExfatfsVolumeLabelEntry *entry,
	const char *label)
{
	unsigned int count = 0;
	uint16_t character;

	entry->entryType = 0x83;
	memset(entry->volumeLabel, 0,
		sizeof(entry->volumeLabel) + sizeof(entry->reserved));
	while (count < 11 && label[count] != 0) {
		character = (int8_t)label[count];
		entry->volumeLabel[count][0] = character;
		entry->volumeLabel[count][1] = character >> 8;
		count++;
	}
	entry->characterCount = count;
}

static inline int _prepare_exfat_root_directory(AppmgrFatFormatContext *context)
{
	AppmgrFatFormatArguments *arguments = context->arguments;
	uint64_t dataOffset = appmgrFatClusterDataOffset(context);
	uint32_t rootOffset, cluster, count = 0;
	uint32_t bitmapFlags = 0;
	unsigned int checksum;
	unsigned char *root;

	if (arguments->bufferSize < dataOffset +
	    appmgrGetFatInitialAllocationSize(context, 1)) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1,
			"%s: buffer_size(%d) is too small, please use larger buffer\n",
			__func__, arguments->bufferSize);
#endif
		return (int)0x8001001c;
	}
	rootOffset = (uint32_t)dataOffset +
		arguments->clusterSize * (context->rootCluster - 2);
	memset((unsigned char *)arguments->buffer + rootOffset, 0,
		arguments->clusterSize);
	cluster = context->bitmapCluster1;
	if (cluster) {
		memset((unsigned char *)arguments->buffer + dataOffset +
			arguments->clusterSize * (cluster - 2), 0,
			appmgrFatBitmapAllocationSize(context));
		root = (unsigned char *)arguments->buffer + rootOffset;
		appmgrFatPrepareBitmapEntry((SceExfatfsAllocationBitmapEntry *)root,
			0, context->bitmapCluster1, context->clusterCount);
		bitmapFlags = 1;
		count = 1;
	}
	cluster = context->bitmapCluster2;
	if (cluster) {
		memset((unsigned char *)arguments->buffer + dataOffset +
			arguments->clusterSize * (cluster - 2), 0,
			appmgrFatBitmapAllocationSize(context));
		root = (unsigned char *)arguments->buffer + rootOffset +
			count * sizeof(SceExfatfsAllocationBitmapEntry);
		appmgrFatPrepareBitmapEntry((SceExfatfsAllocationBitmapEntry *)root,
			bitmapFlags, context->bitmapCluster2, context->clusterCount);
		count++;
	}
	cluster = context->upcaseCluster;
	if (cluster) {
		checksum = appmgrFatUpcaseChecksum();
		memcpy((unsigned char *)arguments->buffer +
			arguments->clusterSize * (cluster - 2) + dataOffset,
			exfatDefaultUpcaseTable, sizeof(exfatDefaultUpcaseTable));
		root = (unsigned char *)arguments->buffer + rootOffset +
			count++ * sizeof(SceExfatfsUpcaseTableEntry);
		appmgrFatPrepareUpcaseEntry((SceExfatfsUpcaseTableEntry *)root,
			checksum, context->upcaseCluster);
	}
	if (context->arguments->volumeLabel) {
		root = (unsigned char *)arguments->buffer + rootOffset +
			count * sizeof(SceExfatfsVolumeLabelEntry);
		appmgrFatPrepareLabelEntry((SceExfatfsVolumeLabelEntry *)root,
			context->arguments->volumeLabel);
	}
	return 0;
}


static inline int _prepare_PBR(AppmgrFatFormatContext *context)
{
	unsigned int type = context->arguments->type;
	SceExfatfsBootSector *boot;

	if (type && type <= 3)
		return _prepare_FAT_PBR(context);
	if (type == 4) {
		boot = context->arguments->buffer;
		memset(boot, 0, context->bytesPerSector);
		appmgrInitializeExfatBootSector(boot, context);
		return 0;
	}
#if FW_DEBUG
	SceDebugForDriver_1A3F2AA4(1, "%s: unsupported type(%d)\n", __func__, type);
#endif
	return (int)0x80010086;
}

static inline int _prepare_image(AppmgrFatFormatContext *context)
{
	AppmgrFatFormatArguments *arguments = context->arguments;
	SceExfatfsBootSector *boot;
	unsigned char *buffer, *fat;
	uint64_t dataOffset;
	uint32_t fatSize, fatOffset, rootSize, infoOffset, backupOffset;
	unsigned int copies, count;
	int result;

	result = _prepare_PBR(context);
	if (result < 0)
		goto failed;
	if (arguments->bufferSize < arguments->initialImageSize)
		return result;
	if (context->reservedSectors > 1)
		memset((unsigned char *)context->arguments->buffer +
			context->bytesPerSector, 0,
			context->bytesPerSector * (context->reservedSectors - 1));
	switch (context->type) {
	case 1:
	case 2:
		fatSize = context->fatSectors * context->bytesPerSector;
		copies = context->fatCount;
		fatOffset = context->reservedSectors * context->bytesPerSector;
		if (copies * fatSize + fatOffset > context->arguments->bufferSize) {
			result = (int)0x8001001c;
			break;
		}
		fat = (unsigned char *)context->arguments->buffer + fatOffset;
		memset(fat, 0, copies * fatSize);
		for (count = copies; count; count--) {
			fat[0] = 0xf8;
			fat[1] = 0xff;
			fat[2] = 0xff;
			fat[3] = context->type == 2 ? 0xff : 0;
			fat += fatSize;
		}
		rootSize = 32 * context->rootEntryCount;
		dataOffset = (context->legacyReservedSectors +
			context->fatSectors * context->fatCount) *
			(uint64_t)context->bytesPerSector;
		if (context->arguments->bufferSize < dataOffset + rootSize) {
			result = (int)0x8001001c;
			break;
		}
		memset((unsigned char *)context->arguments->buffer + dataOffset,
			0, rootSize);
		return 0;
	case 3:
		arguments = context->arguments;
		rootSize = arguments->clusterSize;
		dataOffset = appmgrFatClusterDataOffset(context);
		if (arguments->bufferSize < dataOffset + rootSize) {
			result = (int)0x8001001c;
			break;
		}
		memset((unsigned char *)arguments->buffer + dataOffset, 0,
			rootSize);
		result = _prepare_FAT32_area(context);
		if (result < 0)
			break;
		boot = context->arguments->buffer;
		rootSize = context->bytesPerSector;
		infoOffset = rootSize * boot->fat32.fsInfoSector;
		backupOffset = rootSize * boot->fat32.backupBootSector;
		if (infoOffset + backupOffset + rootSize >
		    context->arguments->bufferSize) {
			result = (int)0x8001001c;
			break;
		}
		buffer = (unsigned char *)boot + infoOffset;
		appmgrPrepareFat32FsInfo(context, infoOffset);
		memcpy(buffer + backupOffset, buffer, rootSize);
		boot = context->arguments->buffer;
		rootSize = context->bytesPerSector;
		backupOffset = rootSize * boot->fat32.backupBootSector;
		if (backupOffset + rootSize > context->arguments->bufferSize) {
			result = (int)0x8001001c;
			break;
		}
		memcpy((unsigned char *)boot + backupOffset, boot, rootSize);
		return 0;
	case 4:
		result = _prepare_exfat_root_directory(context);
		if (result < 0)
			break;
		arguments = context->arguments;
		dataOffset = appmgrFatClusterDataOffset(context);
		if (arguments->bufferSize < dataOffset +
		    appmgrGetFatInitialAllocationSize(context, 1)) {
			result = (int)0x8001001c;
			break;
		}
		if (context->bitmapCluster1)
			appmgrFatInitializeBitmap((unsigned char *)arguments->buffer +
				dataOffset + arguments->clusterSize *
				(context->bitmapCluster1 - 2),
				context->firstUnusedCluster);
		if (context->bitmapCluster2)
			appmgrFatInitializeBitmap((unsigned char *)arguments->buffer +
				dataOffset + arguments->clusterSize *
				(context->bitmapCluster2 - 2),
				context->firstUnusedCluster);
		appmgrSetExfatBootGeometry(arguments->buffer, context);
		result = _prepare_FAT32_area(context);
		if (result < 0)
			break;
		result = appmgrPrepareExfatBootRegion(context->arguments,
			&context->bytesPerSector);
		if (result < 0)
			break;
		rootSize = 24 * context->bytesPerSector;
		if (rootSize > context->arguments->bufferSize) {
			result = (int)0x8001001c;
			break;
		}
		rootSize >>= 1;
		buffer = context->arguments->buffer;
		memcpy(buffer + rootSize, buffer, rootSize);
		return 0;
	default:
		result = (int)0x80010016;
		break;
	}
failed:
	context->arguments->phase = 7;
	return result;
}


static inline int appmgrFatNotifyTransferred(AppmgrFatFormatContext *context, int result)
{
	AppmgrFatFormatArguments *arguments = context->arguments;
	AppmgrFatFormatNotification notification = context->notification;
	int error;

	arguments->writtenDataSize += result;
	if (notification) {
		error = notification(arguments);
		if (error < 0)
			return error;
	}
	return result;
}

static inline int appmgrFatNotifyWrite(AppmgrFatFormatContext *context, int result)
{
	AppmgrFatFormatArguments *arguments;
	AppmgrFatFormatNotification notification;
	int error;

	if (result < 0) {
#if FW_DEBUG
		error_notification(&context->arguments, &context->notification, result);
#else
		if (context->notification)
			context->notification(context->arguments);
#endif
		return result;
	}
	arguments = context->arguments;
	notification = context->notification;
	arguments->writtenDataSize += result;
	if (notification) {
		error = notification(arguments);
		if (error < 0)
			return error;
	}
	return result;
}

static inline int initialize_FAT(AppmgrFatFormatContext *context)
{
	unsigned char *buffer;
	uint32_t bufferSize, remaining, chunk;
	unsigned int copies;
	SceOff seekResult;
	int result = 0;

	seekResult = ksceIoLseek(context->file,
		appmgrFatDeviceOffset(context, (uint64_t)context->bytesPerSector *
			context->reservedSectors), SCE_SEEK_SET);
	if (seekResult < 0) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: sceIoLseek failed[%#llx]\n",
			__func__, seekResult);
#endif
		return seekResult;
	}
	result = seekResult;
	copies = context->fatCount;
	buffer = context->arguments->buffer;
	bufferSize = context->arguments->bufferSize;
	if (bufferSize > context->bytesPerSector) {
		bufferSize -= context->bytesPerSector;
		buffer += context->bytesPerSector;
	}
	memset(buffer, 0, bufferSize);
	while (copies) {
		remaining = context->fatSectors * context->bytesPerSector;
		buffer[0] = 0xf8;
		buffer[1] = 0xff;
		buffer[2] = 0xff;
		buffer[3] = context->arguments->type == 2 ? 0xff : 0;
		chunk = remaining;
		if (bufferSize < chunk)
			chunk = bufferSize;
		result = ksceIoWrite(context->file, buffer, chunk);
		result = appmgrFatNotifyWrite(context, result);
		if (result < 0)
			return result;
		buffer[0] = 0;
		buffer[1] = 0;
		buffer[2] = 0;
		buffer[3] = 0;
		for (;;) {
			remaining -= result;
			if (!remaining)
				break;
			chunk = remaining;
			if (bufferSize < chunk)
				chunk = bufferSize;
			result = ksceIoWrite(context->file, buffer, chunk);
			result = appmgrFatNotifyWrite(context, result);
			if (result < 0)
				return result;
		}
		copies--;
	}
	return result;
}

static inline int clear_root_entries(AppmgrFatFormatContext *context)
{
	unsigned int bufferOffset = context->bytesPerSector;
	int result;

	if (context->arguments->bufferSize <= bufferOffset)
		bufferOffset = 0;
	result = zero_fill(context, bufferOffset, 32 * context->rootEntryCount,
		(uint64_t)(context->legacyReservedSectors +
			context->fatSectors * context->fatCount) *
			context->bytesPerSector, 1);
	if (result < 0) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: zero_fill failed[%#x]\n", __func__,
			result);
#endif
	}
	return appmgrFatNotifyWrite(context, result);
}

static inline int write_info_sector(AppmgrFatFormatContext *context)
{
	AppmgrFatFormatArguments *arguments;
	SceExfatfsBootSector *boot;
	void *buffer;
	unsigned int sectorSize, bufferOffset;
	int64_t infoOffset, backupOffset;
	int result;

	arguments = context->arguments;
	sectorSize = context->bytesPerSector;
	boot = arguments->buffer;
	buffer = boot;
	bufferOffset = 0;
	if (sectorSize < arguments->bufferSize) {
		infoOffset = (int32_t)(sectorSize * boot->fat32.fsInfoSector);
		backupOffset = (int32_t)(sectorSize * boot->fat32.backupBootSector);
		buffer = (unsigned char *)buffer + sectorSize;
		bufferOffset = sectorSize;
	} else {
		infoOffset = sectorSize;
		backupOffset = (int32_t)(6 * sectorSize);
	}
	appmgrPrepareFat32FsInfo(context, bufferOffset);
	result = device_fd_write(context, buffer, context->bytesPerSector,
		appmgrFatDeviceOffset(context, infoOffset));
	if (result < 0) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: default_write failed[%#x]\n",
			__func__, result);
#endif
		return result;
	}
	result = device_fd_write(context, buffer, context->bytesPerSector,
		appmgrFatDeviceOffset(context, backupOffset + infoOffset));
	if (result < 0)
		return result;
	arguments = context->arguments;
	sectorSize = context->bytesPerSector;
	if (sectorSize == arguments->bufferSize) {
		result = _prepare_PBR(context);
		if (result)
			return result;
		sectorSize = context->bytesPerSector;
	}
	boot = arguments->buffer;
	backupOffset = (int32_t)(sectorSize * boot->fat32.backupBootSector);
	return device_fd_write(context, boot, sectorSize,
		appmgrFatDeviceOffset(context, backupOffset));
}


static inline int _write_exfat_root_directory(AppmgrFatFormatContext *context)
{
	AppmgrFatFormatArguments *arguments = context->arguments;
	unsigned int sectorSize = context->bytesPerSector;
	unsigned int bufferSize = arguments->bufferSize;
	unsigned int bufferOffset = bufferSize <= sectorSize ? 0 : sectorSize;
	unsigned int initialUnused, clusterSize, headerSize, count, checksum;
	uint64_t bitmapOffset, rootOffset, offset;
	unsigned char *buffer = (unsigned char *)arguments->buffer + bufferOffset;
	uint32_t cluster;
	int result, transferred;

	cluster = context->bitmapCluster1;
	if (!cluster) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: unexpected bitmap_cluster_1\n",
			__func__);
#endif
		return (int)0x80010030;
	}
	clusterSize = arguments->clusterSize;
	initialUnused = context->firstUnusedCluster;
	bitmapOffset = appmgrFatClusterDataOffset(context) +
		(uint32_t)(clusterSize * (cluster - 2));
	result = zero_fill(context, bufferOffset,
		appmgrFatBitmapAllocationSize(context) * context->fatCount,
		bitmapOffset, 1);
	if (result < 0) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: zero_fill failed[%#x]\n",
			"_initialize_allocation_bitmap", result);
#endif
		result = appmgrFatNotifyWrite(context, result);
		goto update;
	}
	headerSize = sectorSize *
		((sectorSize - 1 + ((initialUnused + 5) >> 3)) / sectorSize);
	transferred = result - context->fatCount * headerSize;
	result = appmgrFatNotifyTransferred(context, transferred);
	if (result < 0)
		goto update;
	if (bufferSize - bufferOffset < headerSize) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: buffer_size(%d) < data_size(%d)\n",
			"_initialize_allocation_bitmap", bufferSize - bufferOffset,
			headerSize);
#endif
		result = (int)0x8001000c;
		goto update;
	}
	memset(buffer, 0, headerSize);
	appmgrFatInitializeBitmap(buffer, context->firstUnusedCluster);
	result = device_fd_write(context, buffer, headerSize,
		appmgrFatDeviceOffset(context, bitmapOffset));
	if (result < 0) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: default_write failed[%#x]\n",
			"_initialize_allocation_bitmap", result);
#endif
		goto update;
	}
	cluster = context->bitmapCluster2;
	if (cluster) {
		offset = bitmapOffset + (uint32_t)(context->arguments->clusterSize *
			(cluster - context->bitmapCluster1));
		result = device_fd_write(context, buffer, headerSize,
			appmgrFatDeviceOffset(context, offset));
		if (result < 0)
			goto update;
	}
	if (context->upcaseCluster) {
		memset(exfatUpcaseWriteBuffer, 0, sizeof(exfatUpcaseWriteBuffer));
		memcpy(exfatUpcaseWriteBuffer, exfatDefaultUpcaseTable,
			sizeof(exfatDefaultUpcaseTable));
		offset = appmgrFatClusterDataOffset(context) +
			(uint32_t)(context->arguments->clusterSize *
				(context->upcaseCluster - 2));
		result = device_fd_write(context, exfatUpcaseWriteBuffer,
			sizeof(exfatUpcaseWriteBuffer),
			appmgrFatDeviceOffset(context, offset));
		if (result < 0) {
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1, "%s: default_write failed[%#x]\n",
				"_write_clusters", result);
#endif
			goto update;
		}
	}
	result = appmgrPrepareFat32RootCluster(context);
	if (result < 0) {
		result = appmgrFatNotifyWrite(context, result);
		goto update;
	}
	sectorSize = context->bytesPerSector;
	transferred = result - sectorSize;
	result = appmgrFatNotifyTransferred(context, transferred);
	if (result < 0)
		goto update;
	rootOffset = appmgrFatClusterDataOffset(context) +
		(uint32_t)(arguments->clusterSize * (context->rootCluster - 2));
	memset(buffer, 0, sectorSize);
	appmgrFatPrepareBitmapEntry((SceExfatfsAllocationBitmapEntry *)buffer,
		0, context->bitmapCluster1, context->clusterCount);
	count = 1;
	cluster = context->bitmapCluster2;
	if (cluster) {
		appmgrFatPrepareBitmapEntry((SceExfatfsAllocationBitmapEntry *)buffer +
			1, 1, cluster, context->clusterCount);
		count = 2;
	}
	cluster = context->upcaseCluster;
	if (cluster) {
		checksum = appmgrFatUpcaseChecksum();
		appmgrFatPrepareUpcaseEntry((SceExfatfsUpcaseTableEntry *)buffer +
			count++, checksum, cluster);
	}
	if (arguments->volumeLabel)
		appmgrFatPrepareLabelEntry((SceExfatfsVolumeLabelEntry *)buffer +
			count, arguments->volumeLabel);
	result = device_fd_write(context, buffer, sectorSize,
		appmgrFatDeviceOffset(context, rootOffset));
update:
	if (initialUnused != context->firstUnusedCluster) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: initial_unused(%d), unused_cluster(%d)\n",
			__func__, initialUnused, context->firstUnusedCluster);
#endif
		sectorSize = context->bytesPerSector;
		headerSize = sectorSize *
			((sectorSize - 1 + ((context->firstUnusedCluster + 5) >> 3)) /
				sectorSize);
		memset(buffer, 0, headerSize);
		appmgrFatInitializeBitmap(buffer, context->firstUnusedCluster);
		offset = appmgrFatClusterDataOffset(context) +
			(uint32_t)(context->arguments->clusterSize *
				(context->bitmapCluster1 - 2));
		result = device_fd_write(context, buffer, headerSize,
			appmgrFatDeviceOffset(context, offset));
		if (result < 0) {
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1, "%s: default_write failed[%#x]\n",
				"_update_cluster_data", result);
#endif
		} else if (!result && context->bitmapCluster2) {
			offset = appmgrFatClusterDataOffset(context) +
				(uint32_t)(context->arguments->clusterSize *
					(context->bitmapCluster2 - 2));
			result = device_fd_write(context, buffer, headerSize,
				appmgrFatDeviceOffset(context, offset));
#if FW_DEBUG
			if (result < 0)
				SceDebugForDriver_1A3F2AA4(1,
					"%s: default_write failed[%#x]\n",
					"_update_cluster_data", result);
#endif
		}
		appmgrSetExfatBootGeometry(arguments->buffer, context);
	}
	return result;
}


static inline int appmgrFatWriteExfatBootRegions(AppmgrFatFormatContext *context)
{
	AppmgrFatFormatArguments *arguments;
	unsigned char *buffer;
	unsigned int sectorSize, bootSize, checksum, index;
	uint64_t offset;
	int result;

	sectorSize = context->bytesPerSector;
	arguments = context->arguments;
	bootSize = (24 * sectorSize) >> 1;
	if (sectorSize >= arguments->bufferSize) {
		buffer = arguments->buffer;
		memset(buffer, 0, context->bytesPerSector);
		appmgrInitializeExfatBootSector((SceExfatfsBootSector *)buffer,
			context);
		sectorSize = context->bytesPerSector;
		buffer = arguments->buffer;
		checksum = 0;
		for (index = 0; index < sectorSize; index++) {
			if (index != 106 && index != 107 && index != 112)
				checksum = appmgrUpdateExfatBootChecksum(checksum,
					&buffer[index], 1);
		}
		result = appmgrWriteExfatExtendedBootRegion(context, checksum);
		if (result < 0)
			return result;
		buffer = context->arguments->buffer;
		memset(buffer, 0, context->bytesPerSector);
		appmgrInitializeExfatBootSector((SceExfatfsBootSector *)buffer,
			context);
		return device_fd_write(context, arguments->buffer,
			context->bytesPerSector,
			appmgrFatDeviceOffset(context, bootSize));
	}
	buffer = arguments->buffer;
	if (bootSize <= arguments->bufferSize) {
		void *afterBoot = buffer + sectorSize;

		appmgrPrepareExfatBootRegion(context->arguments,
			&context->bytesPerSector);
		if (2 * bootSize <= arguments->bufferSize) {
			buffer = arguments->buffer;
			memcpy(buffer + bootSize, buffer, bootSize);
			return device_fd_write(context, afterBoot,
				2 * bootSize - sectorSize,
				appmgrFatDeviceOffset(context, sectorSize));
		}
		result = device_fd_write(context, afterBoot, bootSize - sectorSize,
			appmgrFatDeviceOffset(context, sectorSize));
		if (result < 0)
			return result;
		buffer = arguments->buffer;
		return device_fd_write(context, buffer, bootSize,
			appmgrFatDeviceOffset(context, bootSize));
	}
	checksum = 0;
	for (index = 0; index < sectorSize; index++) {
		if (index != 106 && index != 107 && index != 112)
			checksum = appmgrUpdateExfatBootChecksum(checksum, &buffer[index], 1);
	}
	result = appmgrWriteExfatExtendedBootRegion(context, checksum);
	if (result < 0)
		return result;
	offset = appmgrFatDeviceOffset(context, bootSize);
	return device_fd_write(context, arguments->buffer, context->bytesPerSector,
		offset);
}

static inline int _write_after_PBR(AppmgrFatFormatContext *context)
{
	AppmgrFatFormatArguments *arguments = context->arguments;
	unsigned int sectorSize = context->bytesPerSector;
	unsigned int size = arguments->initialImageSize;
	int result, transferred;

	if (arguments->bufferSize >= size)
		return device_fd_write(context,
			(unsigned char *)arguments->buffer + sectorSize,
			size - sectorSize,
			appmgrFatDeviceOffset(context, (int64_t)(int32_t)sectorSize));
#if FW_DEBUG
	SceDebugForDriver_1A3F2AA4(1,
		"%s: buffer_size[%#x] < initial_image_size[%#x]\n",
		__func__, arguments->bufferSize, size);
#endif
	switch (context->type) {
	case 1:
	case 2:
		result = initialize_FAT(context);
		if (result <= 0)
			return result;
		return clear_root_entries(context);
	case 3:
		context->rootCluster = 2;
		result = appmgrPrepareFat32RootCluster(context);
		if (result < 0)
			return appmgrFatNotifyWrite(context, result);
		transferred = result +
			context->bytesPerSector * (context->legacyReservedSectors - 4);
		result = appmgrFatNotifyTransferred(context, transferred);
		if (result < 0)
			return result;
		result = initialize_FAT32(context);
		if (result < 0)
			return result;
		return write_info_sector(context);
	case 4:
		result = _write_exfat_root_directory(context);
		if (result < 0)
			return result;
		result = initialize_FAT32(context);
		if (result < 0)
			return result;
		return appmgrFatWriteExfatBootRegions(context);
	default:
		return (int)0x80010016;
	}
}

#if FW_DEBUG
int _format_partition(AppmgrFatFormatContext *context)
#else
static inline int _format_partition(AppmgrFatFormatContext *context)
#endif
{
	AppmgrFatFormatArguments *arguments = context->arguments;
	unsigned int imageSize, phase;
	int result = 0;

	if (!arguments->initialImageSize) {
		imageSize = 32 * context->rootEntryCount + context->bytesPerSector *
			(context->reservedSectors + context->fatSectors *
				context->fatCount);
		if (imageSize)
			context->arguments->initialImageSize += imageSize +
				appmgrGetFatInitialAllocationSize(context, 1);
		else {
			result = (int)0x80010016;
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1,
				"%s: _calculate_image_size failed[%#x]\n",
				__func__, result);
#endif
		}
	}
	phase = arguments->phase;
	if (phase == 1) {
		arguments->phase = 2;
		result = zero_fill(context, 0,
			context->reservedSectors * context->bytesPerSector, 0, 1);
		if (result < 0) {
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1, "%s: zero_fill failed[%#x]\n",
				"_invalidate_reserved_area", result);
			SceDebugForDriver_1A3F2AA4(1,
				"%s: _invalidate_reserved_area failed[%#x]\n",
				__func__, result);
#endif
			arguments->phase = 7;
			return result;
		}
		phase = arguments->phase;
	}
	if (phase == 2) {
		arguments->phase = 3;
		result = _prepare_image(context);
		if (result < 0) {
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1, "%s: _prepare_image failed[%#x]\n",
				__func__, result);
#endif
			arguments->phase = 7;
			return result;
		}
		phase = arguments->phase;
	}
	if (phase == 3) {
		arguments->phase = 4;
		result = _write_after_PBR(context);
		if (result < 0) {
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1, "%s: _write_image failed[%#x]\n",
				__func__, result);
#endif
			arguments->phase = 7;
			return result;
		}
		phase = arguments->phase;
	}
	if (phase == 4) {
		unsigned int sectorSize = context->bytesPerSector;
		AppmgrFatFormatArguments *currentArguments = context->arguments;

		arguments->phase = 5;
		if (sectorSize == currentArguments->bufferSize) {
			result = _prepare_PBR(context);
			if (result)
				goto finished;
			sectorSize = context->bytesPerSector;
		}
		result = device_fd_write(context, currentArguments->buffer, sectorSize,
			appmgrFatDeviceOffset(context, 0));
finished:
		if (result >= 0)
			return 0;
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: _update_PBR failed[%#x]\n",
			__func__, result);
#endif
	}
	return result;
}

static inline uint64_t _volume_size(AppmgrFatFormatContext *context, int index)
{
	if (index == 1)
		return context->sectorCount * context->bytesPerSector;
	if (index == 2)
		return context->alternate.sectorCount * context->bytesPerSector;
#if FW_DEBUG
	SceDebugForDriver_1A3F2AA4(1, "%s: unsupported index(%d)\n", __func__, index);
#endif
	return 0;
}

static inline unsigned int _fat_offset(AppmgrFatFormatContext *context, int index)
{
	if (index == 1)
		return context->reservedSectors * context->bytesPerSector;
	if (index == 2)
		return context->alternate.reservedSectors * context->bytesPerSector;
#if FW_DEBUG
	SceDebugForDriver_1A3F2AA4(1, "%s: unsupported index(%d)\n", __func__, index);
#endif
	return 0;
}

int FatFormat(AppmgrFatFormatArguments *arguments)
{
	AppmgrFatFormatContext context;
	uint64_t partitionSize;
#if FW_DEBUG
	uint64_t volumeSize;
#endif
	unsigned int clusterSize, imageSize, type;
	int result, partitionResult, index, task, lastTask;
	int format = 0, calculate = 0;

	result = _check_arguments(arguments, &context, 1);
	if (result != 0)
		return result;
	if (arguments->phase != 8)
		goto format_partition;
	if (arguments->initialImageSize == 512) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: only create partition table\n", __func__);
#endif
	} else {
		format = 1;
		if (!arguments->initialImageSize) {
			calculate = 1;
			arguments->initialImageSize = context.bytesPerSector;
		}
	}
	if (arguments->deviceName != NULL &&
	    appmgrFatPathHasPrefix(arguments->deviceName, "sdstor0:xmc-") &&
	    _fs_util_device_format(arguments->deviceName) >= 0) {
		partitionResult = _fs_util_prepare_rm_mbr(arguments->deviceName,
			context.file, arguments->volumeSize, context.bytesPerSector,
			arguments->buffer, arguments->bufferSize);
		if (partitionResult < 0)
			goto partition_failed;
		partitionSize = appmgrMbrGetPartitionSize(arguments->buffer, 0);
		if (partitionSize) {
			index = 0;
			task = 1;
			do {
				clusterSize = appmgrMbrGetClusterSize(arguments->buffer, index);
				if (clusterSize) {
					_set_start_lba(&context, task,
						appmgrMbrGetPartitionOffset(arguments->buffer, index));
					_set_volume_size(&context, task,
						partitionSize * context.bytesPerSector);
#if FW_DEBUG
					volumeSize = arguments->volumeSize;
					if (partitionSize != volumeSize / context.bytesPerSector)
						SceDebugForDriver_1A3F2AA4(1,
							"%s: change partition size[%#llx] -> [%#llx]\n",
							__func__, volumeSize, _volume_size(&context, task));
#endif
					arguments->volumeSize = _volume_size(&context, task);
#if FW_DEBUG
					if (clusterSize != _cluster_size(&context, task))
						SceDebugForDriver_1A3F2AA4(1,
							"%s: change cluster_size in arguments[%#x] -> [%#x] in partition_conf.h, partition_index(%d), format_task(%d)\n",
							__func__, _cluster_size(&context, task),
							clusterSize, index, task);
#endif
					type = appmgrMbrGetFormatType(arguments->buffer, index);
					partitionResult = appmgrConfigureFatFormat(&context,
						task++, type, clusterSize);
				}
				partitionSize = appmgrMbrGetPartitionSize(arguments->buffer, ++index);
			} while (partitionSize);
		}
		if (partitionResult != 0)
			goto partition_failed;
	}
	partitionResult = device_fd_write(&context, context.arguments->buffer, 512, 0);
	if (partitionResult < 0) {
		context.arguments->phase = 7;
		goto partition_failed;
	}
	if (partitionResult != 512 && partitionResult != 0)
		goto partition_failed;
	if (!format)
		goto format_partition;
	lastTask = 1;
	arguments->phase = 1;
	if (!calculate) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: initial_image_size is already set\n", __func__);
#endif
		arguments->phase = 2;
		goto format_partition;
	}
	imageSize = 32 * context.rootEntryCount + context.bytesPerSector *
		(context.reservedSectors + context.fatSectors * context.fatCount);
	if (!imageSize) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: _add_initial_image_size(%d) failed[%#x]\n",
			__func__, 1, (int)0x80010016);
#endif
		result = (int)0x80010016;
		goto cleanup;
	}
	context.arguments->initialImageSize += imageSize +
		appmgrGetFatInitialAllocationSize(&context, 1);
	if (context.alternate.sectorCount * context.bytesPerSector != 0) {
		imageSize = 32 * context.alternate.rootEntryCount + context.bytesPerSector *
			(context.alternate.reservedSectors +
				context.alternate.fatSectors * context.alternate.fatCount);
		if (!imageSize) {
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1,
				"%s: _add_initial_image_size(%d) failed[%#x]\n",
				__func__, 2, (int)0x80010016);
#endif
			result = (int)0x80010016;
			goto cleanup;
		}
		context.arguments->initialImageSize += imageSize +
			appmgrGetFatInitialAllocationSize(&context, 2);
		lastTask = 2;
	}
	arguments->phase = 2;
	for (task = lastTask; task != 0; task--) {
		int fillResult = zero_fill(&context, 0, _fat_offset(&context, task), 0, task);

		if (fillResult < 0) {
#if FW_DEBUG
			SceDebugForDriver_1A3F2AA4(1, "%s: zero_fill failed[%#x]\n",
				"_invalidate_reserved_area", fillResult);
			SceDebugForDriver_1A3F2AA4(1,
				"%s: _invalidate_reserved_area failed[%#x], index(%d)\n",
				__func__, fillResult, task);
#endif
			result = fillResult;
			arguments->phase = 7;
		}
	}
	if (result != 0)
		goto cleanup;
format_partition:
	result = _format_partition(&context);
	if (result == 0 && context.alternate.startLba != 0) {
		result = (int)0x80010030;
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: index(%d) isn't supported yet\n",
			"_format_partition_n", 2);
#endif
	}
	goto cleanup;
partition_failed:
#if FW_DEBUG
	SceDebugForDriver_1A3F2AA4(1, "%s: create_partition_table failed[%#x]\n",
		__func__, partitionResult);
#endif
	result = partitionResult;
	arguments->phase = 7;
cleanup:
	if (context.arguments->phase != 7)
		context.arguments->phase = 6;
	partitionResult = ksceIoClose(context.file);
	if (partitionResult < 0) {
#if FW_DEBUG
		SceDebugForDriver_1A3F2AA4(1, "%s: sceIoClose failed[%#x]\n",
			"cleanup", partitionResult);
#endif
		context.arguments->phase = 7;
	} else if (context.arguments->phase == 6) {
		context.arguments->phase = 0;
	}
	return result;
}

