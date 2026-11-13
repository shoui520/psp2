# SILK SDK 1.0.8 (fixed point)

Skype's "Fixed Point SILK SDK 1.0.8 beta source code package" (readme.txt dated
15/06/2011), the copy FreeSWITCH carries in `libs/silk`
(https://github.com/signalwire/freeswitch, tag v1.2.0, commit
a7d992a531b3203222911ca632ef61a1bf2b9339). The copyright headers say 2006-2011;
1.0.9 says 2006-2012, and its encoder state differs from the one Venezia uses.

Kept, unchanged: `COPYING`, `readme.txt`, `interface/*.h`, `src/*.c` and `src/*.h`.
Left out: the Visual Studio projects, `Makefile.am`, `configure.*`, `doc/`, `test/`
and `test_vectors/`.

Built by vnzimg (`sys/internal/driver/src/vnzimg/firmware/Makefile`); the files Sony
changed are in its `application/silk/`, marked `SCE`, and replace their copies here.
