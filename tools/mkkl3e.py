#!/usr/bin/env python3
"""KL3E / KL4E range-coded LZ77 compressor and C header emitter for PSP boot payloads.

Produces bit-exact KL3E/KL4E streams compatible with Sony's decodeKL4E() in
sys661/kernel/src/loadexec/kl4e.c and sys661/ipl/kl4e.c.
"""

import argparse
import sys
from pathlib import Path

MODEL_BYTES = 2656
LITERAL_TREE_SIZE = 255
DISTANCE_TREE = 2033
DISTANCE_BITS = 2216
MATCH_MODEL = 2336
LENGTH_LOW = 2400
LENGTH_HIGH = 2424


class RangeEncoder:
    def __init__(self) -> None:
        self.low = 0
        self.range = 0xFFFFFFFF
        self.cache = 0
        self.cache_size = 1
        self.first = True
        self.out = bytearray()

    def _shift_low(self) -> None:
        if self.low < 0xFF000000 or (self.low >> 32) != 0:
            carry = self.low >> 32
            if self.first:
                self.first = False
                # Discard the synthetic leading zero byte before the 4-byte code word
                if self.cache_size > 1:
                    self.out.extend(bytes([(0xFF + carry) & 0xFF]) * (self.cache_size - 1))
            else:
                self.out.append((self.cache + carry) & 0xFF)
                if self.cache_size > 1:
                    self.out.extend(bytes([(0xFF + carry) & 0xFF]) * (self.cache_size - 1))
            self.cache = (self.low >> 24) & 0xFF
            self.cache_size = 1
        else:
            self.cache_size += 1
        self.low = (self.low << 8) & 0xFFFFFFFF

    def normalize(self) -> None:
        if (self.range >> 24) == 0:
            self._shift_low()
            self.range = (self.range << 8) & 0xFFFFFFFF

    def encode_bit(self, probs: bytearray, idx: int, decay: int, inc: int, bit: int) -> None:
        prob = probs[idx]
        self.normalize()
        bound = (self.range >> 8) * prob
        prob -= prob >> decay
        if bit:
            self.range = bound
            prob += inc
        else:
            self.low += bound
            self.range -= bound
        probs[idx] = prob & 0xFF

    def encode_bit_val(self, prob: int, decay: int, inc: int, bit: int) -> int:
        self.normalize()
        bound = (self.range >> 8) * prob
        prob -= prob >> decay
        if bit:
            self.range = bound
            prob += inc
        else:
            self.low += bound
            self.range -= bound
        return prob & 0xFF

    def encode_raw_bits(self, value: int, count: int) -> None:
        self.normalize()
        for shift in range(count - 1, -1, -1):
            bit = (value >> shift) & 1
            self.range >>= 1
            if not bit:
                self.low += self.range

    def finish(self) -> bytes:
        for _ in range(5):
            self._shift_low()
        return bytes(self.out)


def _length_to_class(length: int) -> int:
    if length == 1:
        return -1
    if length <= 3:
        return 0
    if length <= 7:
        return 1
    if length <= 15:
        return 2
    if length <= 31:
        return 3
    if length <= 63:
        return 4
    if length <= 127:
        return 5
    return 6


def _dist_to_class(dist: int) -> int:
    # For distance >= 1:
    # class 0: 1..2, class 1: 3..6, class 2: 7..14, ...
    # value starts at 1, shifted by (c + 1) bits, with last bit subtracting 1 if 0 -> range [2^(c+1)-1 .. 2^(c+2)-2]
    v = dist + 1
    return v.bit_length() - 2


def compress_kl(data: bytes, magic: bytes = b"KL3E", flags: int = 0x17) -> bytes:
    if not data:
        raise ValueError("input must be non-empty")
    lit_shift = flags & 7
    init_prob = 128 - 16 * ((flags >> 3) & 3)
    probs = bytearray([init_prob] * MODEL_BYTES)
    rc = RangeEncoder()

    n = len(data)
    head: dict[bytes, list[int]] = {}
    max_dist = 0x7FFE
    max_len = 254

    def record_pos(p: int) -> None:
        if p + 2 <= n:
            k = data[p:p + 2]
            lst = head.get(k)
            if lst is None:
                head[k] = [p]
            else:
                lst.append(p)
                if len(lst) > 256:
                    del lst[:128]

    def find_match(p: int) -> tuple[int, int]:
        if p + 2 > n or p == 0:
            return 0, 0
        best_len = 0
        best_dist = 0
        best_score = 0
        lim = min(max_len, n - p)
        # Check distance 0 (RLE)
        if data[p - 1] == data[p]:
            l = 1
            while l < lim and data[p + l] == data[p - 1]:
                l += 1
            if l >= 2:
                best_len = l
                best_dist = 0
                best_score = l * 8
        lst = head.get(data[p:p + 2])
        if lst:
            for cand in reversed(lst):
                dist = p - cand - 1
                if dist > max_dist:
                    break
                if dist == 0:
                    continue
                if best_len >= 3 and data[cand + best_len - 1] != data[p + best_len - 1]:
                    continue
                l = 2
                while l < lim and data[cand + l] == data[p + l]:
                    l += 1
                if l < 2:
                    continue
                if l <= 3 and dist > 254:
                    continue
                # Prefer longer matches, penalize distant short matches
                dist_pen = dist.bit_length()
                score = l * 8 - dist_pen
                if l == 2 and dist > 30:
                    continue
                if score > best_score:
                    best_score = score
                    best_len = l
                    best_dist = dist
                    if l == lim and dist < 16:
                        break
        return best_len, best_dist

    def encode_literal(pos: int, prev_sym: int, byte_val: int) -> None:
        ctx = (((prev_sym & 0xFF) | ((pos & 7) << 8)) >> lit_shift) & 7
        base = ctx * LITERAL_TREE_SIZE
        sym = 1
        for shift in range(7, -1, -1):
            b = (byte_val >> shift) & 1
            rc.encode_bit(probs, base + sym - 1, 3, 31, b)
            sym = sym * 2 + b

    def encode_length(pos: int, state: int, length: int) -> int:
        lclass = _length_to_class(length)
        cur_state = state
        for k in range(7):
            is_more = 1 if k <= lclass else 0
            rc.encode_bit(probs, MATCH_MODEL + cur_state + 8, 4, 15, is_more)
            cur_state += 8
            if not is_more:
                break
        if lclass >= 0:
            row = (lclass << 5) | (((pos << lclass) & 3) << 3) | (cur_state & 7)
            # In decodeKL4E, length = ((value << low_bits) | low_part)
            low_bits = min(lclass + 1, 3)
            low_part = length & ((1 << low_bits) - 1)
            value = length >> low_bits
            if lclass >= 3:
                hp = probs[LENGTH_HIGH + row]
                high_bits = lclass - 2
                # top bit of value is always implicit 1
                b0 = (value >> (high_bits - 1)) & 1
                hp = rc.encode_bit_val(hp, 3, 31, b0)
                if lclass > 3:
                    b1 = (value >> (high_bits - 2)) & 1
                    hp = rc.encode_bit_val(hp, 3, 31, b1)
                    if lclass > 4:
                        rc.encode_raw_bits(value & ((1 << (lclass - 4)) - 1), lclass - 4)
                probs[LENGTH_HIGH + row] = hp
            rc.encode_bit(probs, LENGTH_LOW + row, 3, 31, (low_part >> (low_bits - 1)) & 1)
            if lclass > 0:
                rc.encode_bit(probs, LENGTH_LOW + row + 8, 3, 31, (low_part >> (low_bits - 2)) & 1)
                if lclass > 1:
                    rc.encode_bit(probs, LENGTH_LOW + row + 16, 3, 31, low_part & 1)
        return lclass

    def encode_distance(length: int, lclass: int, dist: int) -> None:
        tree_base = lclass if length <= 2 else lclass + 56
        depth = 3 if length <= 2 else 4
        if dist == 0:
            code_idx = 0
        else:
            dclass = _dist_to_class(dist)
            code_idx = dclass + 1
        node = 8
        for shift in range(depth - 1, -1, -1):
            b = (code_idx >> shift) & 1
            rc.encode_bit(probs, DISTANCE_TREE + tree_base + node, 3, 31, b)
            node = node * 2 + b * 8
        if dist == 0:
            return
        dclass = code_idx - 1
        selector = dclass * 8
        model_idx = (DISTANCE_BITS + selector) // 4
        off = model_idx * 4
        packed = int.from_bytes(probs[off:off + 4], "little")
        # Reconstruct bits matching decodeKL4E:
        # value starts at 1, shifted through high bits (if dclass>=3) then last_bit+1 low bits,
        # where the final bit subtracts 1 if 0 (i.e. encodes dist + 1 with bit0 inverted? No:
        # value = (prefix * 2) - (1 if last==0 else 0) => prefix*2 - 1 (bit=0) or prefix*2 (bit=1)!
        # So (dist + 1) = (prefix * 2) + bit - 1  =>  target = dist + 1, final_bit = (dist + 1) & 1 ^ 1?
        # Check: if prefix=1: bit=0 -> 1*2-1 = 1; bit=1 -> 1*2 = 2.
        # So for target = dist: final_bit = 1 if (dist % 2 == 0) else 0, and prefix = (dist + 1) // 2!
        final_bit = 1 if (dist & 1) == 0 else 0
        prefix = (dist + 1) >> 1
        last_bit = dclass if dclass < 2 else 2
        low_prefix_bits = last_bit  # bits before final_bit
        low_prefix = prefix & ((1 << low_prefix_bits) - 1) if low_prefix_bits else 0
        high_val = prefix >> low_prefix_bits
        if dclass >= 3:
            hp = (packed >> 24) & 0xFF
            high_bits = dclass - 2
            b0 = (high_val >> (high_bits - 1)) & 1
            hp = rc.encode_bit_val(hp, 3, 31, b0)
            if dclass > 3:
                b1 = (high_val >> (high_bits - 2)) & 1
                hp = rc.encode_bit_val(hp, 3, 31, b1)
                if dclass > 4:
                    rc.encode_raw_bits(high_val & ((1 << (dclass - 4)) - 1), dclass - 4)
            packed = (packed & 0x00FFFFFF) | (hp << 24)
        for i in range(last_bit + 1):
            lp = (packed >> (i * 8)) & 0xFF
            if i == last_bit:
                b = final_bit
            else:
                b = (low_prefix >> (low_prefix_bits - 1 - i)) & 1
            lp = rc.encode_bit_val(lp, 3, 31, b)
            packed = (packed & ~(0xFF << (i * 8))) | (lp << (i * 8))
        probs[off:off + 4] = packed.to_bytes(4, "little")

    # Note: decodeKL4E ALWAYS emits 1 literal byte first, then checks KL4E_MATCH[state].
    # After a match copy, it copies `length` bytes and then loops back with decodeLiteral=0,
    # emitting `symbol = *copySource` (the byte right after the match!) and checking KL4E_MATCH[state].
    # Wait: let's match kl4e.c's exact state machine!
    # Look at kl4e.c lines 92-105 & 236-278:
    #   On entry: decodeLiteral=1 -> decodes literal `symbol`, writes *output = symbol, checks MATCH[state], output++.
    #   If MATCH==0: state = max(0, state-1), decodeLiteral=1, continue.
    #   If MATCH==1: decodes length & distance, copies `length` bytes from (output - distance - 1),
    #                advances output += length, sets symbol = copySource[length], state = 6 | (output & 1),
    #                and continues with decodeLiteral=0! That means it IMMEDIATELY writes `symbol` (an extra
    #                copied byte, total match copy = length + 1 bytes!) BEFORE checking MATCH[state]!
    # So a match of encoded `length` L actually copies L + 1 bytes total!
    pos = 0
    state = 0
    prev_sym = 0
    need_literal = True

    while pos < n:
        if need_literal:
            b = data[pos]
            encode_literal(pos, prev_sym, b)
            prev_sym = b
            record_pos(pos)
            pos += 1
        need_literal = False

        if pos >= n:
            # Emit end-of-stream match (bit=1, length=255)
            rc.encode_bit(probs, MATCH_MODEL + state, 4, 15, 1)
            encode_length(pos, state, 255)
            break

        # Remember a match of encoded length L copies L + 1 bytes (so max total = 255 -> L = 254).
        # Also if match would hit exact end of buffer, encoded length L = rem - 1 copies all remaining bytes
        # (and then the last byte is emitted at top of loop before EOF check!).
        best_len, best_dist = find_match(pos)
        if best_len >= 2 and best_len < 64 and pos + 1 < n:
            # One-step lazy matching: defer if pos+1 yields a significantly better match
            next_len, _ = find_match(pos + 1)
            if next_len >= best_len + 2:
                best_len = 0
        if best_len >= 2:
            enc_len = best_len - 1
            if enc_len == 255:
                enc_len = 254
                best_len = 255
            if enc_len <= 2 and best_dist > 254:
                best_len = 0
            if best_len >= 2 and best_dist > 65534:
                best_len = 0

        if best_len >= 2:
            rc.encode_bit(probs, MATCH_MODEL + state, 4, 15, 1)
            lclass = encode_length(pos, state, enc_len)
            encode_distance(enc_len, lclass, best_dist)
            for k in range(best_len):
                record_pos(pos + k)
            copy_end = pos + enc_len
            prev_sym = data[copy_end]
            pos += best_len
            state = 6 | (copy_end & 1)
            # decodeLiteral stays 0: top of next iteration checks pos >= n or emits next match/literal!
        else:
            rc.encode_bit(probs, MATCH_MODEL + state, 4, 15, 0)
            if state != 0:
                state -= 1
            need_literal = True

    return magic + bytes([flags]) + rc.finish()


def main() -> int:
    ap = argparse.ArgumentParser(description="KL3E/KL4E compress a binary and optionally emit a C header.")
    ap.add_argument("input", type=Path)
    ap.add_argument("-o", "--output", type=Path, required=True)
    ap.add_argument("--magic", default="KL3E", help="4-byte header magic (KL3E or KL4E)")
    ap.add_argument("--c-array", default=None, help="Emit C header defining this array name instead of raw bytes")
    ap.add_argument("--pad-to", type=int, default=0, help="Pad output to exact byte size")
    args = ap.parse_args()

    raw = args.input.read_bytes()
    comp = compress_kl(raw, magic=args.magic.encode("ascii"))
    if args.pad_to:
        if len(comp) > args.pad_to:
            print(f"error: compressed size {len(comp)} exceeds --pad-to {args.pad_to}", file=sys.stderr)
            return 1
        comp = comp + bytes(args.pad_to - len(comp))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.c_array:
        lines = [
            "/* Auto-generated from C-compiled reboot.bin by tools/mkkl3e.py. Do not edit. */",
            f"unsigned char __attribute__((aligned(64))) {args.c_array}[{len(comp)}] = {{",
        ]
        for i in range(0, len(comp), 16):
            chunk = ", ".join(f"0x{b:02x}" for b in comp[i:i + 16])
            lines.append(f"    {chunk},")
        lines.append("};")
        args.output.write_text("\n".join(lines) + "\n")
    else:
        args.output.write_bytes(comp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
