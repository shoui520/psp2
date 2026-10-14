# SFMT 1.3.3

Pristine upstream source: http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/SFMT/SFMT-src-1.3.3.tar.gz
(sha256 f9d7a0044d0123a4655db02ca60af1d807155c6e98e2bec2a3f7bb48d2dca597), by Mutsuo Saito
and Makoto Matsumoto, BSD license (LICENSE.txt). Only the files the MEXP=19937 build needs are kept,
unchanged.

SceSysmem's KernelUtils SFMT (ksceSfmt19937*) is NEON assembly in the original; it computes this
algorithm with a per-call context. The adaptation is the patch src/os0/kd/sysmem/source/sfmt_sce.patch.
The SFMT19937 output sequence is the same in every upstream version, so the version choice does not
change behaviour.
