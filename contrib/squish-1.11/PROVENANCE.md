# squish 1.11

Pristine upstream source: `squish-1.11.zip`, Simon Brown's libsquish release 1.11
(sha256 6e13120e2ec5fa34a116cafc23e48585ca7e963870b39bcd0bbe7df30aafa6a4).
The library sources, headers, `config.h`, `singlecolourlookup.inl`, README and
ChangeLog are copied unchanged; `extra/`, the Makefile, `config`, the Xcode and
Visual Studio projects, the plist and the S3TC specification text are left out.
LICENSE is the MIT notice at the top of every source file.

Version pinned from libpaf's `CompressMasked`: its flag fix-up keeps the colour
metric flags (`kColourMetricPerceptual`, `kColourMetricUniform`) and folds
`kColourIterativeClusterFit` into `kColourClusterFit`, which is 1.11's
`FixFlags`; 1.13 dropped the metric flags for a metric pointer, and 1.14 moved
the fit flags for BC4/BC5.
