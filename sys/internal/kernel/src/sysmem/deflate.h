/*
 * The inflater's symbol encodings (deflate.S, deflate_huffman.S).
 *
 * A literal/length symbol is a halfword: a length without extra bits is
 * 0x800 plus the length; one with extra bits keeps their count in the low
 * byte and the two bits below the leading one of its base in bits 8-9, the
 * base being ((4 + those bits) << extra) + 3. A distance symbol is a byte:
 * 30 minus its extra bits, shifted left by two, plus the bits below the
 * leading one of its base.
 */
#pragma once

#define DEFLATE_LENGTH(length)			(0x800 + (length))
#define DEFLATE_LENGTH_EXTRA(extra, mantissa)	(0xc00 + ((mantissa) << 8) + (extra))
#define DEFLATE_END_OF_BLOCK			DEFLATE_LENGTH_EXTRA(0, 0)
#define DEFLATE_INVALID				DEFLATE_LENGTH_EXTRA(0, 1)
#define DEFLATE_DISTANCE(extra, mantissa)	(((30 - (extra)) << 2) + (mantissa))
